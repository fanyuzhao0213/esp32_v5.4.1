set(src_dirs
            LED
            EXIT
            ESPTIMER
            TIMG
            PWM
            MYIIC
            XL9555
            AT24C02
            ADC
            AP3216C
            RMT_RX
            RMT_TX
            SPIFFS)

set(include_dirs
            LED
            EXIT
            ESPTIMER
            TIMG
            PWM
            MYIIC
            XL9555
            AT24C02
            ADC
            AP3216C
            RMT_RX
            RMT_TX
            SPIFFS)

set(requires
            driver
            esp_timer
            esp_adc
            spiffs)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)