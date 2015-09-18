################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/home/michael/network_systems/hw1/webserver.cpp 

C_SRCS += \
/home/michael/network_systems/hw1/webserver.c 

OBJS += \
./hw1/webserver.o 

C_DEPS += \
./hw1/webserver.d 

CPP_DEPS += \
./hw1/webserver.d 


# Each subdirectory must supply rules for building sources it contributes
hw1/webserver.o: /home/michael/network_systems/hw1/webserver.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

hw1/webserver.o: /home/michael/network_systems/hw1/webserver.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/home/michael/network_systems/hw1 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


