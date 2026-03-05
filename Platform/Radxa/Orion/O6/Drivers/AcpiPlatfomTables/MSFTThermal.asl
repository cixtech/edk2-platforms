/*
 * Copyright 2026 - Cix Technology Group Co., Ltd. All Rights Reserved.
 *
*/

#include <Include/AcpiScmi.h>

External (UDBG, MethodObj)

/*Features Enable/Disable Control*/
#define MPTF_THERMAL_ENABLE 0
#define MPTF_BATTERY_AND_PSU_ENABLE 0
#define MPTF_POWERLIMIT_ENABLE 0
#define MPTF_CUSTOMIZE_IO_SIGNALSS_ENABLE 0
// For Debugger
#define MPTF_POWER_TRACKER 0

/*Drivers support Control
  It is controlled by enabling MPTF features.

For example:
  If enable Thermal feature,
  MSFT_TEMPERATURE_SENSOR_DRIVER_SUPPORT, MSFT_MPTF_CORE_DRIVER_SUPPORT, MSFT_THERMAL_POLICY_CLIENT_DRIVER_SUPPORT, MSFT_POWER_LIMIT_CLIENT_DRIVER_SUPPORT
  will be support.
*/

// MSFT:
#define MSFT_TEMPERATURE_SENSOR_DRIVER_SUPPORT (MPTF_THERMAL_ENABLE || 0)
#define MSFT_CUSTOMIZED_IO_DRIVER_SUPPORT (MPTF_CUSTOMIZE_IO_SIGNALSS_ENABLE || 0)
#define MSFT_MPTF_CORE_DRIVER_SUPPORT (MPTF_POWERLIMIT_ENABLE || MPTF_BATTERY_AND_PSU_ENABLE || MPTF_CUSTOMIZE_IO_SIGNALSS_ENABLE || MPTF_THERMAL_ENABLE || 0)
#define MSFT_THERMAL_POLICY_CLIENT_DRIVER_SUPPORT (MPTF_THERMAL_ENABLE || 0)
#define MSFT_POWER_LIMIT_CLIENT_DRIVER_SUPPORT (MPTF_POWERLIMIT_ENABLE || MPTF_THERMAL_ENABLE || 0)
#define MSFT_SIGNAL_IO_CLIENT_DRIVER_SUPPORT (MPTF_CUSTOMIZE_IO_SIGNALSS_ENABLE || 0)
#define MSFT_POWER_SOURCE_CLIENT_DRIVER_SUPPORT (MPTF_BATTERY_AND_PSU_ENABLE || 0)
// CIX:
#define DOMAIN_SOC0_SUPPORT (MPTF_POWERLIMIT_ENABLE || 0)

/*Device*/
#if MSFT_TEMPERATURE_SENSOR_DRIVER_SUPPORT
// Skin temperature sensor
Device(TMPT) {
  Name(_HID, "MSFT000A")
  Name (_UID, 0)
  Name (_STA, 0xF)

  Name(TCUR, 0x0)    // Currently TMP
  Name(TMIN, 0x0)    // MIN TMP
  Name(TMAX, 0x0)    // MAX TMP

  Method(_TMP, 0, Serialized)
  {
    Name(BUF0, Buffer(9){0xDA,0x03,0xB3,0x3E,0x0C,0x00,0x00,0x00,0x00})
    Name(BUF1, Buffer(12){})

    if(\_SB.EC0.TRAS(BUF0,Sizeof(BUF0),BUF1,Sizeof(BUF1)) == 0){
      CreateByteField (BUF1, 0x0A, TMPI)  //Integral part of temperature
      CreateByteField (BUF1, 0x0B, TMPF)  //Temperature fractional part, accuracy 0.01
      TMPI = ToInteger(TMPI)
      TMPF = ToInteger(TMPF)
      //To degrees Kelvin
      Multiply(TMPI,10,Local0)
      Divide(TMPF, 10, , Local1)
      Add(Local0,Local1,Local0)
      Add(Local0,2732,Local0)
      Store(Local0, TCUR)
      Return(Local0)
    }
    Return(Zero)
  }

  // Arg0 GUID
  //      1f0849fc-a845-4fcf-865c-4101bf8e8d79 - Temperature GUID
  // Arg1 Revision
  // Arg2 Function Index
  // Arg3 Function dependent
  Method(_DSM, 0x4, Serialized) {
    If(LEqual(ToUuid("1f0849fc-a845-4fcf-865c-4101bf8e8d79"),Arg0)) {
      Switch(Arg2) {
        Case(0) {
          // We support function 0,1
          Return (Buffer() {0x03, 0x00, 0x00, 0x00})
        }
        // Update Thresholds
        // Arg3 = Package () { LowTemp, HighTemp }
        Case(1) {
            TMIN = DeRefOf(Index(Arg3, 0))
            TMAX = DeRefOf(Index(Arg3, 1))

            Return(Buffer() {0x00, 0x00, 0x00, 0x00})
        }
      }
    }

    Return (Ones)
  }
}

#endif

#if MSFT_CUSTOMIZED_IO_DRIVER_SUPPORT
// Microsoft Customized IO Driver
Device(CIO1) {
  Name(_HID, "MSFT000B")
  Name (_UID, 0)
  Name (_STA, 0xF)


  // Arg0 GUID
  //      07ff6382-e29a-47c9-ac87-e79dad71dd82 - Input
  //      d9b9b7f3-2a3e-4064-8841-cb13d317669e - Output
  // Arg1 Revision
  // Arg2 Function Index
  // Arg3 Function dependent

  Method(_DSM, 0x4, Serialized) {
    // input variable
    If(LEqual(ToUuid("07ff6382-e29a-47c9-ac87-e79dad71dd82"),Arg0)) {
        Switch(Arg2) {
          Case(0) {
            Return (Buffer() {0x00, 0x00, 0x00, 0x00})
          }
        }
        Return(Ones)
    }
    // 0: success
    // 1: Failure, invalid parameter
    // 2: Failure, unsupported version
    // 3: Failure, hardware error
    // output variable
    // output: OS -> Platform
    If(LEqual(ToUuid("d9b9b7f3-2a3e-4064-8841-cb13d317669e"),Arg0)) {
        Switch(Arg2) {
          Case(0) {
            Return (Buffer() {0x00, 0x00, 0x00, 0x00})
          }
        }
        Return(Ones)
    }

    Return (Ones)
  }
}
#endif

#if MSFT_MPTF_CORE_DRIVER_SUPPORT
// MPTFCore Driver
Device(MPCT) {
  Name(_HID, "MSFT000D")
  Name (_UID, 0)
  Name (_STA, 0xF)
}
#endif

// Maybe TZ here
#if MSFT_THERMAL_POLICY_CLIENT_DRIVER_SUPPORT
  THERMALZone (TPOL) {
    Name (_HID, "MSFT000E")
    Name (_UID, 0x0)
    Name (_STA, 0xF)

    Method(_DSM, 0x4, Serialized) {
      If(LEqual(ToUuid("DE3180F9-CFBD-4FE8-BD22-711DEFF93DFC"),Arg0)) {
        Switch(Arg2) {
          Case(0) {
            Return (Buffer() {0x03, 0x00, 0x00, 0x00})
          }
          Case (1) {
              // Arg3 is Status from THERMAL Policy Client Driver
              Return (Zero)
          }
        }
      }

      Return(Ones)
    }

  }
#endif

#if MSFT_POWER_LIMIT_CLIENT_DRIVER_SUPPORT
// MPTF Power Limit Client Driver
Device (PLCD) {

  Name (_HID, "MSFT000F")

  Name (_UID, 0x0)

  Name (_STA, 0xf)
}
#endif

#if MSFT_POWER_SOURCE_CLIENT_DRIVER_SUPPORT
Device(MPSC) {
  Name(_HID, "MSFT0010")
  Name (_UID, 0)
  Name (_STA, 0xF)
}
#endif

#if MSFT_SIGNAL_IO_CLIENT_DRIVER_SUPPORT
// MPTF Signal IO Client driver
Device(MPSI) {
  Name(_HID, "MSFT0011")
  Name (_UID, 0)
  Name (_STA, 0xF)
}
#endif

#if DOMAIN_SOC0_SUPPORT
Device (SOC0) {
    Name (_HID, "CIXHA037")
    Name (_UID, 0)
    Name (_STA, 0xF)
}
#endif

#if MPTF_POWER_TRACKER
Device (MTPT) {
    Name (_HID, "MSFT0012")
    Name (_UID, 0)
    Name (_STA, 0xF)
}
#endif