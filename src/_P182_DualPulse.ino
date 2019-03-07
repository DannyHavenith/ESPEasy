#ifdef USES_P182
//#######################################################################################################
//#################################### Plugin 182: Pulse  ###############################################
//#######################################################################################################

#define PLUGIN_182
#define PLUGIN_ID_182         182
#define PLUGIN_NAME_182       "Asymmetric Dual Pulse"
#define PLUGIN_VALUENAME1_182 "LeftCount"
#define PLUGIN_VALUENAME2_182 "LeftTime"
#define PLUGIN_VALUENAME3_182 "RightCount"
#define PLUGIN_VALUENAME4_182 "RightTime"

//this takes 20 bytes of IRAM per handler
void Plugin_182_pulse_interrupt1_primary() ICACHE_RAM_ATTR;
void Plugin_182_pulse_interrupt1_secondary() ICACHE_RAM_ATTR;

namespace Plugin_182_NS
{

  /**
  * Class that implements an asymmetric dual pulse counter.
  * asymmetric dual pulse consists of two pulse signals triggered by a rotating
  * axis. The pulses are at an angle other than 180 degrees (hence asymmetric).
  * One pulse is arbitrarily chosen as the primary pulse and another is the secondary.
  * This device will count the number of subsequent primary pulses if and only if
  * they were interleaved with secondary pulses.
  *
  * The time between primary pulses is counted, together with the time between
  * the secondary pulse and the primary. If the secondary pulse is closer in time
  * to the first primary pulse, the device is, again arbitrarily, assumed to
  * have rotated "left", otherwise it is assumed to have rotated "right".
  *
  * When queried, this device will report for each direction (left, right):
  *    * the number of rotations (from primary to primary) since the last query
  *    * the average time per rotation.
  */
  class PulseState
  {
  public:
    PulseState() = default;

    /**
    * This method will be called on every primary pulse.
    */
    void PulsePrimary()
    {
      const auto now = millis();
      const auto pulseTime = timeDiff(pulseTimePrimary, now);
      if ( pulseTime < debounceTimeMs) return;

      if (pulseTime <= inactivityTimeout)
      {
        if (not active)
        {
          active = true;
        }
        else if (pulseTimePrimary != pulseTimeSecondary)
        {
          const auto firstStretch = timeDiff( pulseTimePrimary, pulseTimeSecondary);
          const auto secondStretch = timeDiff( pulseTimeSecondary, now);
          if (firstStretch > secondStretch)
          {
            ++runningValues.left.counter;
            runningValues.left.time += pulseTime;
          }
          else
          {
            ++runningValues.right.counter;
            runningValues.right.time += pulseTime;
          }
        }
      }
      pulseTimePrimary = now;
      pulseTimeSecondary = now;
    }

    /**
    * This method will be called on every secondary pulse.
    */
    void PulseSecondary()
    {
      const auto now = millis();
      if (pulseTimeSecondary != pulseTimePrimary)
      {
        // we're passing the secondary for the second time,
        // probably direction has changed. Reset primary.
        pulseTimePrimary = now;
      }
      pulseTimeSecondary = now;

    }


    struct Measurements
    {
      unsigned long counter; //< number of rotations
      unsigned long time;    //< time in ms between first and last primary
    };

    struct Values
    {
      Measurements left;
      Measurements right;
    };

    Values GetValues( bool reset = true)
    {
      Values result = runningValues;
      if (reset) runningValues = {};
      return result;
    }
  private:
    static constexpr int  debounceTimeMs = 10; // hard-coded for now.
    // disregard the last primary pulse if this time out has passed.
    static constexpr int  inactivityTimeout = 50000;

    Values        runningValues = {};
    unsigned long pulseTimePrimary = 0;
    unsigned long pulseTimeSecondary = 0;
    bool active = false;
  };

  // We allocate TASKS_MAX, but in reality, to save on interrupt handlers,
  // only the last initialized pulse will be active.
  PulseState states[ TASKS_MAX];
  byte activeTask = 0;

  // divide enumerator by denominator but return 0 if the denominator is 0
  unsigned int SafeDiv( unsigned int enumerator, unsigned int denominator)
  {
    return denominator?(enumerator/denominator):0;
  }

  // return average time per rotation given a Measurement.
  unsigned int TimePerCount( const PulseState::Measurements &m)
  {
    return SafeDiv( m.time, m.counter);
  }
}

boolean Plugin_182(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_182;
        Device[deviceCount].Type = DEVICE_TYPE_DUAL;
        Device[deviceCount].VType = SENSOR_TYPE_QUAD;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 4;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_182);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_182));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_182));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_182));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_182));
        break;
      }

    case PLUGIN_GET_DEVICEGPIONAMES:
      {
        event->String1 = formatGpioName_input(F("Primary"));
        event->String2 = formatGpioName_input(F("Secondary"));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        // ignored.
      	addFormNumericBox(F("Debounce Time (mSec)"), F("p182_debounce")
      			, PCONFIG(0));

        // int modeValues[4];
        // modeValues[0] = LOW;
        // modeValues[1] = CHANGE;
        // modeValues[2] = RISING;
        // modeValues[3] = FALLING;

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        PCONFIG(0) = getFormItemInt(F("p182_debounce"));
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SHOW_VALUES:
      {
        const auto values = Plugin_182_NS::states[event->TaskIndex].GetValues( false);
        string += F("<div class=\"div_l\">");
        string += ExtraTaskSettings.TaskDeviceValueNames[0];
        string += F(":</div><div class=\"div_r\">");
        string += values.left.counter;
        string += F("</div><div class=\"div_br\"></div><div class=\"div_l\">");
        string += ExtraTaskSettings.TaskDeviceValueNames[1];
        string += F(":</div><div class=\"div_r\">");
        string += TimePerCount( values.left);
        string += F("</div>ms<div class=\"div_br\"></div><div class=\"div_l\">");
        string += ExtraTaskSettings.TaskDeviceValueNames[2];
        string += F(":</div><div class=\"div_r\">");
        string += values.right.counter;
        string += F("</div><div class=\"div_br\"></div><div class=\"div_l\">");
        string += ExtraTaskSettings.TaskDeviceValueNames[3];
        string += F(":</div><div class=\"div_r\">");
        string += TimePerCount( values.right);
        string += F("</div>ms");
        success = true;
        break;
      }
    case PLUGIN_INIT:
      {
        String log = F("INIT : Pulse ");
        log += CONFIG_PIN1;
        log += F(", ");
        log += CONFIG_PIN2;
        addLog(LOG_LEVEL_INFO,log);
        pinMode(CONFIG_PIN1, INPUT_PULLUP);
        pinMode(CONFIG_PIN2, INPUT_PULLUP);
        success = Plugin_182_pulseinit(CONFIG_PIN1, CONFIG_PIN2, event->TaskIndex);
        break;
      }

    case PLUGIN_READ:
      {
        const auto values = Plugin_182_NS::states[event->TaskIndex].GetValues();
        event->sensorType = SENSOR_TYPE_QUAD;
        UserVar[event->BaseVarIndex + 0] = values.left.counter;
        UserVar[event->BaseVarIndex + 1] = TimePerCount( values.left);
        UserVar[event->BaseVarIndex + 2] = values.right.counter;
        UserVar[event->BaseVarIndex + 3] = TimePerCount( values.right);

        success = true;
        break;
      }
  }
  return success;
}


/*********************************************************************************************\
 * Pulse Counter IRQ handlers
\*********************************************************************************************/
void Plugin_182_pulse_interrupt1_primary()
{
  using namespace Plugin_182_NS;
  states[activeTask].PulsePrimary();
}

void Plugin_182_pulse_interrupt1_secondary()
{
  using namespace Plugin_182_NS;
  states[activeTask].PulseSecondary();
}


/**
* Init Dual pulse. This will deactivate all other dual pulse devices.
*/
bool Plugin_182_pulseinit(byte Par1, byte Par2, byte Index)
{

  Plugin_182_NS::activeTask = Index; // KLUDGE
  attachInterrupt( digitalPinToInterrupt( Par1), Plugin_182_pulse_interrupt1_primary, RISING);
  attachInterrupt( digitalPinToInterrupt( Par2), Plugin_182_pulse_interrupt1_secondary, RISING);

  return true;
}
#endif // USES_P182
