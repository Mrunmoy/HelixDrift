if(SB_CONFIG_BOARD_NRF52840DONGLE_NRF52840 OR
   SB_CONFIG_BOARD_NRF52840DONGLE_NRF52840_BARE)
  set(PM_STATIC_YML_FILE
      ${CMAKE_CURRENT_LIST_DIR}/pm_static_nrf52840dongle_nrf52840.yml
      CACHE INTERNAL "")
elseif(SB_CONFIG_BOARD_PROMICRO_NRF52840_NRF52840)
  set(PM_STATIC_YML_FILE
      ${CMAKE_CURRENT_LIST_DIR}/pm_static_promicro_nrf52840_nrf52840.yml
      CACHE INTERNAL "")
endif()

set(mcuboot_board_extra_conf "")

if(SB_CONFIG_BOARD_NRF52840DONGLE_NRF52840 OR
   SB_CONFIG_BOARD_NRF52840DONGLE_NRF52840_BARE)
  set(mcuboot_board_extra_conf
      ${CMAKE_CURRENT_LIST_DIR}/sysbuild/mcuboot/boards/nrf52840dongle_nrf52840.conf)
elseif(SB_CONFIG_BOARD_PROMICRO_NRF52840_NRF52840)
  set(mcuboot_board_extra_conf
      ${CMAKE_CURRENT_LIST_DIR}/sysbuild/mcuboot/boards/promicro_nrf52840_nrf52840.conf)
endif()

if(mcuboot_board_extra_conf)
  list(APPEND mcuboot_EXTRA_CONF_FILE "${mcuboot_board_extra_conf}")
  list(REMOVE_DUPLICATES mcuboot_EXTRA_CONF_FILE)
  set(mcuboot_EXTRA_CONF_FILE "${mcuboot_EXTRA_CONF_FILE}" CACHE INTERNAL "")
endif()
