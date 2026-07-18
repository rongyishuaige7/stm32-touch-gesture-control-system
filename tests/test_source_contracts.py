from pathlib import Path
import re,unittest
ROOT=Path(__file__).resolve().parents[1]
def read(rel):return (ROOT/rel).read_text(encoding='utf-8')
class SourceContracts(unittest.TestCase):
 def test_platform_and_runtime_clock_are_explicit(self):
  self.assertIn('platform = ststm32@19.5.0',read('platformio.ini'))
  main=read('src/main.cpp')
  self.assertIn('RCC_SYSCLKSOURCE_HSI',main)
  self.assertIn('RCC_PLL_NONE',main)
 def test_source_pin_contract(self):
  cfg=read('include/config.h');esp=read('src/esp8266.cpp')
  for x in ['TOUCH_PIN_UP              GPIO_PIN_0','TOUCH_PIN_LEFT            GPIO_PIN_1','TOUCH_PIN_RIGHT           GPIO_PIN_2','TOUCH_PIN_DOWN            GPIO_PIN_3','APDS9960_I2C_SCL_PIN      GPIO_PIN_6','APDS9960_I2C_SDA_PIN      GPIO_PIN_7']:
   self.assertIn(x,cfg)
  self.assertIn('GPIO_PIN_9',esp);self.assertIn('GPIO_PIN_10',esp)
 def test_softap_is_a_documented_example_not_external_wifi(self):
  cfg=read('include/config.h');readme=read('README.md')
  self.assertIn('#define ESP_WIFI_SSID             "TouchGesture"',cfg)
  self.assertIn('#define ESP_WIFI_PASS             "12345678"',cfg)
  self.assertIn('公开的示例热点配置，不是用户真实 Wi-Fi 凭据',readme)
  self.assertNotIn('AT+CWJAP',read('src/esp8266.cpp'))
 def test_i2c_and_apds_identity_contract(self):
  src=read('src/apds9960.cpp')
  self.assertIn('hi2c1.Init.ClockSpeed      = 100000U',src)
  self.assertIn('APDS9960_I2C_ADDR_7BIT << 1',src)
  for value in ['0xABU','0xA8U','0x9CU','0xA1U']:self.assertIn(value,src)
 def test_brightness_bounds_do_not_underflow_or_overflow(self):
  main=read('src/main.cpp')
  self.assertIn('brightness >= PWM_STEP',main)
  self.assertIn('brightness <= (PWM_MAX_DUTY - PWM_STEP)',main)
  self.assertIn('LED_SetBrightness(LED_CH1, PWM_MIN_DUTY)',main)
  self.assertIn('LED_SetBrightness(LED_CH1, PWM_MAX_DUTY)',main)
 def test_unknown_http_path_is_real_404(self):
  src=read('src/esp8266.cpp')
  self.assertIn('ESP_SendHTTPResponseStatus(conn_id, 404, "Not Found"',src)
  self.assertIn('"HTTP/1.1 %u %s\\r\\n"',src)
 def test_dashboard_is_read_only_and_uses_real_api(self):
  html=read('src/html_page.cpp')
  self.assertIn("fetch('/api/state')",html)
  self.assertNotIn('SYSTEM ONLINE',html.upper())
  self.assertNotRegex(html,r"fetch\('/api/(?:control|led|set)")
 def test_project_orientation_is_available(self):
  status=read('docs/PROJECT_STATUS.md')
  self.assertIn('# 项目说明',status)
  self.assertIn('供电、电平、网络、认证或执行器',status)
if __name__=='__main__':unittest.main()
