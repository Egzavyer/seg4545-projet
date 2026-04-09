################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/app_tasks.c \
../Core/Src/debug.c \
../Core/Src/drv_dht11.c \
../Core/Src/drv_ds18b20.c \
../Core/Src/drv_esp_uart.c \
../Core/Src/drv_lcd_i2c.c \
../Core/Src/drv_max30102.c \
../Core/Src/drv_mpu6050.c \
../Core/Src/drv_mq2.c \
../Core/Src/esp_uart_test.c \
../Core/Src/event_log.c \
../Core/Src/freertos.c \
../Core/Src/main.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_hal_timebase_tim.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/ui.c 

OBJS += \
./Core/Src/app_tasks.o \
./Core/Src/debug.o \
./Core/Src/drv_dht11.o \
./Core/Src/drv_ds18b20.o \
./Core/Src/drv_esp_uart.o \
./Core/Src/drv_lcd_i2c.o \
./Core/Src/drv_max30102.o \
./Core/Src/drv_mpu6050.o \
./Core/Src/drv_mq2.o \
./Core/Src/esp_uart_test.o \
./Core/Src/event_log.o \
./Core/Src/freertos.o \
./Core/Src/main.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_hal_timebase_tim.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/ui.o 

C_DEPS += \
./Core/Src/app_tasks.d \
./Core/Src/debug.d \
./Core/Src/drv_dht11.d \
./Core/Src/drv_ds18b20.d \
./Core/Src/drv_esp_uart.d \
./Core/Src/drv_lcd_i2c.d \
./Core/Src/drv_max30102.d \
./Core/Src/drv_mpu6050.d \
./Core/Src/drv_mq2.d \
./Core/Src/esp_uart_test.d \
./Core/Src/event_log.d \
./Core/Src/freertos.d \
./Core/Src/main.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_hal_timebase_tim.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/ui.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app_tasks.cyclo ./Core/Src/app_tasks.d ./Core/Src/app_tasks.o ./Core/Src/app_tasks.su ./Core/Src/debug.cyclo ./Core/Src/debug.d ./Core/Src/debug.o ./Core/Src/debug.su ./Core/Src/drv_dht11.cyclo ./Core/Src/drv_dht11.d ./Core/Src/drv_dht11.o ./Core/Src/drv_dht11.su ./Core/Src/drv_ds18b20.cyclo ./Core/Src/drv_ds18b20.d ./Core/Src/drv_ds18b20.o ./Core/Src/drv_ds18b20.su ./Core/Src/drv_esp_uart.cyclo ./Core/Src/drv_esp_uart.d ./Core/Src/drv_esp_uart.o ./Core/Src/drv_esp_uart.su ./Core/Src/drv_lcd_i2c.cyclo ./Core/Src/drv_lcd_i2c.d ./Core/Src/drv_lcd_i2c.o ./Core/Src/drv_lcd_i2c.su ./Core/Src/drv_max30102.cyclo ./Core/Src/drv_max30102.d ./Core/Src/drv_max30102.o ./Core/Src/drv_max30102.su ./Core/Src/drv_mpu6050.cyclo ./Core/Src/drv_mpu6050.d ./Core/Src/drv_mpu6050.o ./Core/Src/drv_mpu6050.su ./Core/Src/drv_mq2.cyclo ./Core/Src/drv_mq2.d ./Core/Src/drv_mq2.o ./Core/Src/drv_mq2.su ./Core/Src/esp_uart_test.cyclo ./Core/Src/esp_uart_test.d ./Core/Src/esp_uart_test.o ./Core/Src/esp_uart_test.su ./Core/Src/event_log.cyclo ./Core/Src/event_log.d ./Core/Src/event_log.o ./Core/Src/event_log.su ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_hal_timebase_tim.cyclo ./Core/Src/stm32f4xx_hal_timebase_tim.d ./Core/Src/stm32f4xx_hal_timebase_tim.o ./Core/Src/stm32f4xx_hal_timebase_tim.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/ui.cyclo ./Core/Src/ui.d ./Core/Src/ui.o ./Core/Src/ui.su

.PHONY: clean-Core-2f-Src

