/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "os.h"
#include "os_queue.h"
#include "os_event.h"
#include <stdio.h>
#include <string.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
void task_led(void);
void task_uart(void);
void task_sender(void);
void task_receiver(void);
void task_worker(void);
void handle_command(const char *cmd);
void task_cmd(void);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void task_greedy(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


#define CMD_BUF_SIZE 32
static char isr_buf[CMD_BUF_SIZE];
static char pending_cmd[CMD_BUF_SIZE] = {0};
static uint8_t cmd_pending = 0;
static uint8_t isr_idx   = 0;
static char cmd_buf[CMD_BUF_SIZE];
static uint8_t cmd_ready = 0;
static uint8_t rx_byte   = 0;
static volatile uint8_t uart_busy = 0;
static void uart_send(const uint8_t *data, uint16_t len);
OSQueue test_q;
OSEvent test_event;

static void uart_send(const uint8_t *data, uint16_t len) {
    while (uart_busy) { os_yield(); }
    uart_busy = 1;
    HAL_UART_Transmit(&huart2, (uint8_t*)data, len, HAL_MAX_DELAY);
    uart_busy = 0;
}

void uart_log(const char *msg) {
    static char buf[80];
    uint32_t t = os_now_ms();
    int len = snprintf(buf, sizeof(buf), "[%6lu ms] %s\r\n", t, msg);
    uart_send((uint8_t*)buf, len);
}
void handle_command(const char *cmd) {
    static char out[512];
    int  pos = 0;
    uint32_t t = os_now_ms();
    if (strcmp(cmd, "help") == 0) {
        pos += snprintf(out+pos, sizeof(out)-pos,
            "[%6lu ms] commands:\r\n"
            "[%6lu ms]   help   : show this message\r\n"
            "[%6lu ms]   status : show OS status\r\n"
            "[%6lu ms]   tasks  : show all task states\r\n"
            "[%6lu ms]   latest : show latest queue message\r\n",
            t, t, t, t, t);
    } else if (strcmp(cmd, "status") == 0) {
        pos += snprintf(out+pos, sizeof(out)-pos,
            "[%6lu ms] OS running | tick=%lu ms | tasks=%d\r\n",
            t, t, os_task_count());
    } else if (strcmp(cmd, "tasks") == 0) {
        pos += snprintf(out+pos, sizeof(out)-pos,
            "[%6lu ms] --- task list ---\r\n", t);
        for (uint8_t i = 0; i < os_task_count(); i++) {
            const char *s;
            switch (os_task_get_state(i)) {
                case TASK_READY:    s = "READY";    break;
                case TASK_RUNNING:  s = "RUNNING";  break;
                case TASK_SLEEPING: s = "SLEEPING"; break;
                case TASK_BLOCKED:  s = "BLOCKED";  break;
                default:            s = "UNKNOWN";  break;
            }
            pos += snprintf(out+pos, sizeof(out)-pos,
                "[%6lu ms]   [%d] %-12s %s\r\n",
                t, i, os_task_get_name(i), s);
        }
        pos += snprintf(out+pos, sizeof(out)-pos,
            "[%6lu ms] -----------------\r\n", t);
    } else if (strcmp(cmd, "latest") == 0) {
        pos += snprintf(out+pos, sizeof(out)-pos,
            "[%6lu ms] last event: queue send + event signal every 2s\r\n", t);
    } else {
        pos += snprintf(out+pos, sizeof(out)-pos,
            "[%6lu ms] unknown command: %s\r\n", t, cmd);
    }
    if (pos > 0) {
        uart_send((uint8_t*)out, pos);
    }
}

void task_led(void) {
    while (1) {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        os_sleep_ms(500);
    }
}

void task_uart(void) {
    while (1) {
        uart_log("heartbeat");
        os_sleep_ms(1000);
    }
}

void task_sender(void) {
    while (1) {
        char msg[16] = "hello";
        os_queue_send(&test_q, msg, sizeof(msg));
        uart_log("queue: sending hello");
        os_event_signal(&test_event);
        os_sleep_ms(2000);
    }
}

void task_receiver(void) {
    while (1) {
        char buf[16] = {0};
        if (os_queue_recv(&test_q, buf, sizeof(buf))) {
            uart_log("queue: received hello");
        }
        os_sleep_ms(20);
    }
}

void task_worker(void) {
    while (1) {
        os_event_wait(&test_event);
        uart_log("event: task unblocked!");
    }
}

void task_cmd(void) {
    while (1) {
        if (cmd_ready) {
            strncpy(pending_cmd, cmd_buf, CMD_BUF_SIZE);
            cmd_ready = 0;
            char echo[CMD_BUF_SIZE + 8];
            snprintf(echo, sizeof(echo), ">>> %s", pending_cmd);
            uart_log(echo);
            handle_command(pending_cmd);
        }
        os_sleep_ms(50);
    }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (rx_byte == '\r' || rx_byte == '\n') {
            if (isr_idx > 0 && !cmd_ready) {
                isr_buf[isr_idx] = '\0';
                memcpy(cmd_buf, isr_buf, isr_idx + 1);
                cmd_ready = 1;
                isr_idx   = 0;
            }
        } else if (isr_idx < CMD_BUF_SIZE - 1) {
            isr_buf[isr_idx++] = (char)rx_byte;
        }

        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

void task_greedy(void) {
    uint32_t count = 0;
    while (1) {
        count++;
        if (count % 10000000 == 0) {
            uart_log("greedy: still running (preempted by OS)");
        }
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    /* USER CODE BEGIN 2 */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    os_queue_init(&test_q);
    os_event_init(&test_event);
    os_init();
    os_task_create(task_led,"LED");
    os_task_create(task_uart,"UART");
    os_task_create(task_sender,"SENDER");
    os_task_create(task_receiver,"RECEIVER");
    os_task_create(task_worker,"WORKER");
    os_task_create(task_cmd,"CMD");
    os_task_create(task_greedy,"GREEDY");
    os_start();
    /* USER CODE END 2 */

    while (1) {}
}/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
