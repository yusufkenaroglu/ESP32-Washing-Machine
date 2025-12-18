idf_component_register(
    SRCS
    "main.cpp"
    "drivers/gpio_hal/gpio_hal.cpp"
    "ulp/ulp_manager.cpp"
    "drivers/display/display.cpp"
    "drivers/display/qrcodegen.cpp"
    "drivers/sound/sound.cpp"
    "drivers/mpu6050/mpu6050.cpp"
    "drivers/odrive/odrive.cpp"
    "drivers/wifi/wifi_manager.cpp"
    "drivers/freehome/freehome_manager.cpp"
    "simulator/simulator.cpp"
    "ui_controller/ui_controller.cpp"
    "machine_state/machine_state.cpp"
    "machine_state/constants.cpp"
    "wash_plan/wash_plan.cpp"
    "tasks/tasks.cpp"
    INCLUDE_DIRS 
    "."
    "drivers"
    "drivers/display"
    "drivers/freehome"
    "drivers/gpio_hal"
    "drivers/sound"
    "drivers/mpu6050"
    "drivers/odrive"
    "drivers/wifi"
    "simulator"
    "ulp"
    "machine_state"
    "wash_plan"
    "tasks"
    "ui_controller"
    REQUIRES
    soc 
    nvs_flash 
    ulp 
    esp_driver_gpio 
    esp_driver_i2c
    esp_timer
    esp_event
    esp_http_server
    PRIV_REQUIRES
    esp_driver_dac
    esp_wifi
    app_update
    esp_https_ota
)
#
# ULP support additions to component CMakeLists.txt.
#
# 1. The ULP app name must be unique (if multiple components use ULP).
set(ulp_app_name ulp_${COMPONENT_NAME})
#
# 2. Specify all assembly source files.
#    Files should be placed into a separate directory (in this case, ulp/),
#    which should not be added to COMPONENT_SRCS.
set(ulp_s_sources "ulp/buttons.S" "ulp/wake_up.S")
#
# 3. List all the component source files which include automatically
#    generated ULP export file, ${ulp_app_name}.h:
set(ulp_exp_dep_srcs "ulp_manager.cpp")
#
# 4. Call function to build ULP binary and embed in project using the argument
#    values above.
ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")
