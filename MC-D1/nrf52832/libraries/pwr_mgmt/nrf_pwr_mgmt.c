/* Copyright (c) 2016 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "sdk_common.h"
#if NRF_MODULE_ENABLED(NRF_PWR_MGMT)

#include "nrf_pwr_mgmt.h"
#include "nrf.h"
#include "app_error.h"
#include "nrf_assert.h"
#include "nrf_log_ctrl.h"
#include "app_util_platform.h"

#define NRF_LOG_MODULE_NAME "NRF_PWR_MGMT"
#if NRF_PWR_MGMT_CONFIG_LOG_ENABLED
    #define NRF_LOG_LEVEL       NRF_PWR_MGMT_CONFIG_LOG_LEVEL
    #define NRF_LOG_INFO_COLOR  NRF_PWR_MGMT_CONFIG_INFO_COLOR
    #define NRF_LOG_DEBUG_COLOR NRF_PWR_MGMT_CONFIG_DEBUG_COLOR
#else
    #define NRF_LOG_LEVEL       0
#endif // NRF_PWR_MGMT_CONFIG_LOG_ENABLED
#include "nrf_log.h"

#ifdef SOFTDEVICE_PRESENT
    #include "nrf_soc.h"
    #include "softdevice_handler.h"
#endif // SOFTDEVICE_PRESENT

#if NRF_PWR_MGMT_CONFIG_USE_SCHEDULER
    #if (APP_SCHEDULER_ENABLED != 1)
        #error "APP_SCHEDULER is required."
    #endif
    #include "app_scheduler.h"
#endif // NRF_PWR_MGMT_CONFIG_USE_SCHEDULER

#if ((NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED == 1)   \
  || (NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED   == 1)   \
  || (NRF_PWR_MGMT_CONFIG_AUTO_SHUTDOWN_RETRY       == 1))
    #if (APP_TIMER_ENABLED != 1)
        #error "APP_TIMER is required."
    #endif
    #include "app_timer.h"
    #define APP_TIMER_REQUIRED  1
#else
    #define APP_TIMER_REQUIRED  0
#endif

#if (!defined(__FPU_PRESENT) || (__FPU_PRESENT != 1))       \
 && (NRF_PWR_MGMT_CONFIG_FPU_SUPPORT_ENABLED == 1)
    #error "FPU is required by NRF_PWR_MGMT_CONFIG_FPU_SUPPORT_ENABLED"
#endif

#if NRF_PWR_MGMT_CONFIG_DEBUG_PIN_ENABLED
    #include "nrf_gpio.h"
    #define DEBUG_PIN_CONFIG() nrf_gpio_cfg_output(NRF_PWR_MGMT_SLEEP_DEBUG_PIN)
    #define DEBUG_PIN_CLEAR()  nrf_gpio_pin_clear(NRF_PWR_MGMT_SLEEP_DEBUG_PIN)
    #define DEBUG_PIN_SET()    nrf_gpio_pin_set(NRF_PWR_MGMT_SLEEP_DEBUG_PIN)
    #define DEBUG_PINS_INIT()   \
        do {                    \
            DEBUG_PIN_CLEAR();  \
            DEBUG_PIN_CONFIG(); \
        } while(0)
#else
    #define DEBUG_PIN_CLEAR()
    #define DEBUG_PIN_SET()
    #define DEBUG_PINS_INIT()
#endif

#if (NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED == 1)    \
 || (NRF_PWR_MGMT_CONFIG_DEBUG_PIN_ENABLED == 1)

__STATIC_INLINE void nrf_pwr_mgmt_sleep_init(void)
{
#ifdef SOFTDEVICE_PRESENT
    ASSERT(current_int_priority_get() >= APP_IRQ_PRIORITY_LOW);
#endif
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;
}

    #define SLEEP_INIT()        nrf_pwr_mgmt_sleep_init()
    #define SLEEP_LOCK()        CRITICAL_REGION_ENTER()
    #define SLEEP_RELEASE()     CRITICAL_REGION_EXIT()
#else
    #define SLEEP_INIT()
    #define SLEEP_LOCK()
    #define SLEEP_RELEASE()
#endif

// Create section "pwr_mgmt_data".
//lint -esym(526, pwr_mgmt_dataBase) -esym(526, pwr_mgmt_dataLimit)
NRF_SECTION_VARS_CREATE_SECTION(pwr_mgmt_data, nrf_pwr_mgmt_shutdown_handler_t);

// Helper macros for section variables.
#define PWR_MGMT_SECTION_VARS_GET(i)    NRF_SECTION_VARS_GET((i),                               \
                                                             nrf_pwr_mgmt_shutdown_handler_t,   \
                                                             pwr_mgmt_data)
#define PWR_MGMT_SECTION_VARS_COUNT     NRF_SECTION_VARS_COUNT(nrf_pwr_mgmt_shutdown_handler_t, \
                                                               pwr_mgmt_data)

#if NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    static uint8_t  m_max_cpu_usage;   /**< Maximum observed CPU usage (0 - 100%). */
    static uint32_t m_ticks_sleeping;  /**< Number of ticks spent in sleep mode (__WFE()). */
    static uint32_t m_ticks_last;      /**< Number of ticks from the last CPU usage computation. */
#endif // NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED


#if NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED
    static uint16_t m_standby_counter;     /**< Number of seconds from the last activity
                                                (@ref pwr_mgmt_feed). */
#endif // NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED

static nrf_pwr_mgmt_evt_t   m_pwr_mgmt_evt;     /**< Event type which will be passed to the shutdown
                                                     handlers.*/
static bool                 m_sysoff_guard;     /**< True if application started the shutdown
                                                     preparation. */
static uint8_t              m_next_handler;     /**< Index of the next executed handler. */

#if APP_TIMER_REQUIRED
    APP_TIMER_DEF(m_pwr_mgmt_timer);            /**< Timer used by this module. */
#endif // APP_TIMER_REQUIRED

#if APP_TIMER_REQUIRED
/**@brief Handle events from m_pwr_mgmt_timer.
 */
static void nrf_pwr_mgmt_timeout_handler(void * p_context)
{
#if NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    uint32_t delta;
    uint32_t ticks;
    uint8_t  cpu_usage;

    ticks = app_timer_cnt_get();
    UNUSED_VARIABLE(app_timer_cnt_diff_compute(ticks, m_ticks_last, &delta));
    cpu_usage = 100 * (delta - m_ticks_sleeping) / delta;

    NRF_LOG_DEBUG("CPU Usage: %d%%\r\n", cpu_usage);
    if (m_max_cpu_usage < cpu_usage)
    {
        m_max_cpu_usage = cpu_usage;
    }

    m_ticks_last        = ticks;
    m_ticks_sleeping    = 0;
#endif // NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED

#if NRF_PWR_MGMT_CONFIG_AUTO_SHUTDOWN_RETRY
    if (m_sysoff_guard)
    {
        // Try to continue the shutdown procedure.
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_CONTINUE);
        return;
    }
#endif // NRF_PWR_MGMT_CONFIG_AUTO_SHUTDOWN_RETRY

#if NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED
    if (m_standby_counter < NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_S)
    {
        m_standby_counter++;
    }
    else
    {
        nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
#endif // NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED
}
#endif // APP_TIMER_REQUIRED


ret_code_t nrf_pwr_mgmt_init(uint32_t ticks_per_1s)
{
    NRF_LOG_INFO("Init\r\n");
    DEBUG_PINS_INIT();
    SLEEP_INIT();

#if NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    m_max_cpu_usage     = 0;
    m_ticks_sleeping    = 0;
    m_ticks_last        = 0;
#endif // NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED

#if NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED
    m_standby_counter   = 0;
#endif // NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED
    m_sysoff_guard      = false;
    m_next_handler      = 0;

#if APP_TIMER_REQUIRED
    ret_code_t ret_code = app_timer_create(&m_pwr_mgmt_timer,
                                           APP_TIMER_MODE_REPEATED,
                                           nrf_pwr_mgmt_timeout_handler);
    VERIFY_SUCCESS(ret_code);

    return app_timer_start(m_pwr_mgmt_timer, ticks_per_1s, NULL);
#else
    return NRF_SUCCESS;
#endif // APP_TIMER_REQUIRED
}


void nrf_pwr_mgmt_run(void)
{
#if NRF_PWR_MGMT_CONFIG_FPU_SUPPORT_ENABLED

    CRITICAL_REGION_ENTER();
    uint32_t fpscr = __get_FPSCR();
    /* 
     * Clear FPU exceptions.
     * Without this step, the FPU interrupt is marked as pending,
     * preventing system from sleeping. Exceptions cleared:
     * - IOC - Invalid Operation cumulative exception bit.
     * - DZC - Division by Zero cumulative exception bit.
     * - OFC - Overflow cumulative exception bit.
     * - UFC - Underflow cumulative exception bit.
     * - IXC - Inexact cumulative exception bit.
     * - IDC - Input Denormal cumulative exception bit.
     */
    __set_FPSCR(fpscr & ~0x9Fu);
    __DMB();
    NVIC_ClearPendingIRQ(FPU_IRQn);
    CRITICAL_REGION_EXIT();

    /* 
     * Assert if a critical FPU exception is signaled:
     * - IOC - Invalid Operation cumulative exception bit.
     * - DZC - Division by Zero cumulative exception bit.
     * - OFC - Overflow cumulative exception bit.
     */
    ASSERT((fpscr & 0x03) == 0);
#endif // NRF_PWR_MGMT_CONFIG_FPU_SUPPORT_ENABLED

    SLEEP_LOCK();

#if NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    uint32_t sleep_start;
    uint32_t sleep_end;
    uint32_t sleep_duration;

    sleep_start = app_timer_cnt_get();
#endif // NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED

    DEBUG_PIN_SET();

    // Wait for an event.
#ifdef SOFTDEVICE_PRESENT
    if (softdevice_handler_is_enabled())
    {
        ret_code_t ret_code = sd_app_evt_wait();
        ASSERT((ret_code == NRF_SUCCESS) || (ret_code == NRF_ERROR_SOFTDEVICE_NOT_ENABLED));
    }
    else
#endif // SOFTDEVICE_PRESENT
    {
        __WFE();
    }

    DEBUG_PIN_CLEAR();

#if NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    sleep_end = app_timer_cnt_get();
    UNUSED_VARIABLE(app_timer_cnt_diff_compute(sleep_end,
                                               sleep_start,
                                               &sleep_duration));
    m_ticks_sleeping += sleep_duration;
#endif // NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED

    SLEEP_RELEASE();
}


#if NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED
void nrf_pwr_mgmt_feed(void)
{
    NRF_LOG_DEBUG("Feed\r\n");
    // It does not stop started shutdown process.
    m_standby_counter = 0;
}
#endif // NRF_PWR_MGMT_CONFIG_STANDBY_TIMEOUT_ENABLED


/**@brief Function runs the shutdown procedure.
 */
static void shutdown_process(void)
{
    NRF_LOG_INFO("Shutdown started. Type %d\r\n", m_pwr_mgmt_evt);
    // Executing all callbacks.
    while (m_next_handler < PWR_MGMT_SECTION_VARS_COUNT)
    {
        if ((*PWR_MGMT_SECTION_VARS_GET(m_next_handler))(m_pwr_mgmt_evt))
        {
            NRF_LOG_INFO("SysOff handler 0x%08X => ready\r\n",
                        (unsigned int)*PWR_MGMT_SECTION_VARS_GET(m_next_handler));
        }
        else
        {
            // One of the modules is not ready.
            NRF_LOG_INFO("SysOff handler 0x%08X => blocking\r\n",
                        (unsigned int)*PWR_MGMT_SECTION_VARS_GET(m_next_handler));
            return;
        }

        // Mark handler as executed.
        m_next_handler++;
    }

#if NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    NRF_LOG_INFO("Maximum CPU usage: %u%%\r\n", m_max_cpu_usage);
#endif // NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
    NRF_LOG_WARNING("Shutdown\r\n");
    NRF_LOG_FINAL_FLUSH();

    // Enter System OFF.
#ifdef SOFTDEVICE_PRESENT
    ret_code_t ret_code = sd_power_system_off();
    if (ret_code == NRF_ERROR_SOFTDEVICE_NOT_ENABLED)
    {
        NRF_POWER->SYSTEMOFF = POWER_SYSTEMOFF_SYSTEMOFF_Enter;
    }
    else
    {
        APP_ERROR_CHECK(ret_code);
    }
#else
    NRF_POWER->SYSTEMOFF = POWER_SYSTEMOFF_SYSTEMOFF_Enter;
#endif // SOFTDEVICE_PRESENT
}


#if NRF_PWR_MGMT_CONFIG_USE_SCHEDULER
/**@brief Handle events from app_scheduler.
 */
static void scheduler_shutdown_handler(void * p_event_data, uint16_t event_size)
{
    UNUSED_PARAMETER(p_event_data);
    UNUSED_PARAMETER(event_size);
    shutdown_process();
}
#endif // NRF_PWR_MGMT_CONFIG_USE_SCHEDULER


/**@brief Function for locking shutdown guard. It prevents a multiple call of @ref pwr_mgmt_shutdown
 *        and overwriting the shutdown type.
 */
static bool nrf_pwr_mgmt_guard_lock(void)
{
    bool success = false;

    CRITICAL_REGION_ENTER();
    if (m_sysoff_guard == false)
    {
        m_sysoff_guard = true;
        success        = true;
    }
    CRITICAL_REGION_EXIT();

    return success;
}


void nrf_pwr_mgmt_shutdown(nrf_pwr_mgmt_shutdown_t shutdown_type)
{
    if (shutdown_type != NRF_PWR_MGMT_SHUTDOWN_CONTINUE)
    {
        // Check if shutdown procedure is not started.
        if (!nrf_pwr_mgmt_guard_lock())
        {
            return;
        }
        m_pwr_mgmt_evt = (nrf_pwr_mgmt_evt_t)shutdown_type;
    }
    ASSERT(m_sysoff_guard);
    NRF_LOG_INFO("Shutdown request %d\r\n", shutdown_type);

#if NRF_PWR_MGMT_CONFIG_USE_SCHEDULER
    ret_code_t ret_code = app_sched_event_put(NULL, 0, scheduler_shutdown_handler);
    APP_ERROR_CHECK(ret_code);
#else
    shutdown_process();
#endif // NRF_PWR_MGMT_CONFIG_USE_SCHEDULER
}

#endif // NRF_MODULE_ENABLED(NRF_PWR_MGMT)
