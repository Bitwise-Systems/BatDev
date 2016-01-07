//
//    BatDevT.ino -- NiMH battery charger with TLynx power module
//
//    Version 4: First stripboard build, comprising TLynx with 1024-step
//               level setting potentiometer & ideal diode backflow prevention,
//               plus INA219 voltage/current monitor & dual thermometers.
//
//    Version 4.0: Port previous application to new drivers
//
//    Version 4.1: Changed Duemilanove bootloader to OptiBoot, gaining 1500 bytes
//
//    Version 4.2: Inaugurate use of new virtual timer package
//
//    Version 4.3: Switch smoothing filter from Savitzky-Golay to Gaussian
//
//    Version 4.4: Support for loadable scripts
//
//

#include <EEPROM.h>
#include <DallasTemperature.h>
#include <Utility.h>
#include <FastDigital.h>
#include <BatDevLocale.h>
#include "Drivers.h"
#include "DriverTimer.h"
#include "Smoothing.h"

#undef  Calibrate                  // Do NOT include calibration code

#define MAmpCeiling   3000         // Max allowable thru INA219 shunt resistor
#define MaxBatTemp    40.9         // Max allowed battery temperature
#define MaxBatVolt    1.72         // Max allowed battery voltage
#define ChargeTemp    8.0          // Charge is complete if delta T exceeds 8.0 deg C


//-------------- Record types -------------

#define typeCC         0     // CTRecord format: shuntMA, busV, thermLoad, thermAmbient, millisecs
#define typeCV         1     // Use CTRecord format
#define typeDetect     2     // Dip Detected; CTRecord format
#define typeTherm      3     // Use CTRecord format
#define typeRampUp     4     // Used in ConstantCurrent during ramp-up phase
#define typePulse      5     // Presently unused
#define typeDischarge  9     // Use CTRecord format
#define typeEnd       10     // Elapsed time, exitStatus
#define typeProvEnd   11     // not written yet, use EndRecord format
#define typeJugs      12     // not written yet
#define typeNudge     13     // nudgeCount, potLevel, millis
#define typeIRes      14     // Internal Resistance record, ohms

#define BandPlus     7.0     // ConstantCurrent upper band width
#define BandMinus    3.0     // ConstantCurrent lower band width


//---------------- Globals ----------------

char battID[20] = "<undefined>";          // See '~/MSR/BatDevInventory.py'
int capacity = 2400;                      // Battery capacity in mAh

struct DispatchTable {
    const char *command;                  // Command name
    exitStatus (*handler)(char **);       // Pointer to command handler
};


const struct DispatchTable commandTable[] = {
    { "b",          SetID           },
    { "bc",         SetCapacity     },
    { "bp",         BatPresentCmd   },
    { "bt",         BatteryTypeCmd  },
    { "cc",         ccCmd           },
    { "comp",       CompileCmd      },
    { "cv",         cvCmd           },
    { "d",          DischargeCmd    },
    { "getpga",     GetPgaCmd       },
    { "heat",       ReportHeats     },
    { "help",       PrintHelp       },
    { "iget",       iGetCmd         },
    { "list",       ListCmd         },
    { "load",       LoadCmd         },        // <<< EVALUATING >>>
    { "memq",       MemQCmd         },        // <<< TESTING >>>
    { "nudge",      NudgeCmd        },
    { "off",        PwrOffCmd       },
    { "on",         PwrOnCmd        },
    { "pgood",      PwrGoodCmd      },
    { "power",      ExternalPowerQ  },
    { "r",          ResistCmd       },
    { "ram",        FreeRam         },
    { "run",        RunCmd          },
//  { "s",          Sleep219        },
    { "setpga",     PgaCmd          },
    { "tell",       Report          },
    { "thermo",     ThermLoop       },
    { "ver",        VersionCmd      },
    { "vget",       vGetCmd         },
    { "vset",       VsetCmd         },
    {  NULL,        unknownCommand  }   // Insert additional commands BEFORE this line
};


void setup (void)
{
    Serial.begin(38400);
    delay(600);

    InitTLynx();
    if (Init219() == false) {
        Printf("INA219 not found!\n");
        while (1)
            ;
    }
    if (InitThermo() != 0) {
        Printf("Thermometers not found!\n");
        while (1)
            ;
    }
    InitTimerTask(RefreshTemperatures);
    InitLoads();
    SetPGA(8);

    VersionCmd(NULL);
    SetID(NULL);        // Print the default battery ID as a
                        // ...reminder to set the actual one.
#if Locale == Hutch
    ExternalPowerQ(NULL);
#endif
}


void loop (void)
{
    int i;
    char **t;
    exitStatus rc;

//  Printf("> ");
    Printf("\x1B[1G\x1B[0K> ");    // cursor to column 1, and erase to end of line
    t = util.GetCommand();
    if (*t == NULL)
        return;
    for (i = 0; commandTable[i].command != NULL; i++)
        if (strcmp(commandTable[i].command, *t) == 0)
            break;

    rc = (*commandTable[i].handler)(t);    // call command handler

    if (rc != 0) {
        ReportExitStatus(rc);
		Printf("\n");
    }
    if (strcmp(commandTable[i].command, "run") == 0)
        Printf("~");

}
