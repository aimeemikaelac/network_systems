################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../ConcurrentQueue.o \
../webserver.o \
../webserver_util.o 

CPP_SRCS += \
../ConcurrentQueue.cpp \
../webserver.cpp \
../webserver_util.cpp 

OBJS += \
./ConcurrentQueue.o \
./webserver.o \
./webserver_util.o 

CPP_DEPS += \
./ConcurrentQueue.d \
./webserver.d \
./webserver_util.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


