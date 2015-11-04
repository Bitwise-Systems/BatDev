//
//    BatDevT.ino -- NiMH battery charger with TLynx power module
//
//    Version 4: First stripboard build, comprising TLynx with 1024-step
//               level setting potentiometer & ideal diode backflow prevention,
//               plus INA219 voltage/current monitor & dual thermometers.
//
//    Version 4.0: Port previous application to new drivers
//
//
//    Version 4.1: Changed Duemilanove bootloader to OptiBoot, gaining ~1500 bytes
//                 Update eepromhelp.ino
//
//    Version 4.2: Inaugurate use of new virtual timer package
//
//

#include <EEPROM.h>
#include <DallasTemperature.h>
#include <Utility.h>
#include "Drivers.h"
#include "DriverTimer.h"
#include "Savitzky.h"

#undef Calibrate                   // Do NOT include calibration code

#define vThresh       1.52         // Sets min voltage to start dip detection
#define mAmpCeiling   4000         // Max allowable thru INA219 shunt resistor
#define maxBatTemp    40.9         // Max allowed battery temperature
#define maxBatVolt    1.70         // Max allowed battery voltage

#define typeCCRecord         0     // CTRecord format: shuntMA, busV, thermLoad, thermAmbient, millisecs
#define typeCVRecord         1     // Use CTRecord format
#define typeDetectRecord     2     // Dip Detected; CTRecord format
#define typeThermRecord      3     // Use CTRecord format
#define typeRampUpRecord     4     // Used in ConstantCurrent during ramp-up phase
#define typePulseRecord      5     // Taken during 'off' pulse while charging    CTRecord format
#define typeDischargeRecord  9     // Use CTRecord format, not written yet
#define typeEndRecord       10     // Elapsed time, exitStatus
#define typeProvEndRecord   11     // not written yet, use EndRecord format
#define typeJugsRecord      12     // not written yet
#define typeNudgeRecord     13     // nudgeCount, potLevel, millis
#define IResRec             14     // Internal Resistance record, ohms

#define bandPlus  7.0              // ConstantCurrent upper band width
#define bandMinus 3.0              // ConstantCurrent lower band width


//-------------globals-------------

char battID[20] = "<undefined>";

boolean scriptrunning = false;            // Regulates issuance of '~' to close external records file

typedef struct DispatchTable {
    const char *command;                  // Command name
    exitStatus (*handler)(char **);       // Pointer to command handler
};


const struct DispatchTable commandTable[] = {
    { "b",          SetID           },
    { "bp",         BatPresentCmd   },
    { "ccd",        ccdCmd          },  // constantcurrent, dual bands
    { "cv",         cvCmd           },
    { "d",          DischargeCmd    },
    { "getpga",     GetPgaCmd       },
    { "heat",       ReportHeats     },
    { "help",       PrintHelp       },
    { "iget",       iGetCmd         },
    { "loff",       LoffCmd         },  // Load off
    { "lon",        LonCmd          },  // Load on, args h, m, l, b [hi, med, lo, bus]
//  { "internal",   IntResistCmd    },
//  { "jugs",       ReportJugs      },
    { "nudge",      NudgeCmd        },
    { "off",        PwrOffCmd       },
    { "on",         PwrOnCmd        },
    { "pgood",      PwrGoodCmd      },
//  { "rate",       ChargeRateCmd   },
    { "ram",        FreeRam         },
//  { "s",          Sleep219        },
    { "script",     ScriptCmd       },
    { "setpga",     PgaCmd          },
    { "tell",       Report          },
    { "thermo",     ThermLoop       },  // Access to 'ThermMonitor'
//  { "trial",      ConstCurrTrial  },
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
        Printf("INA219 breakout board not found!");
        while (1)
            ;
    }
    if (InitThermo() != 0) {
        Printf("Thermometers not found!");
        while (1)
            ;
    }
    InitTimerTask(RefreshTemperatures);
    InitLoads();
    SetPGA(8);

    FreeRam(NULL);  // Keep tabs on amount of free RAM.
    SetID(NULL);    // Print the default battery ID as a
                    // ...reminder to set the actual one.
}


void loop (void)
{
    int i;
    exitStatus rc;
    char **t;

    t = util.GetCommand();
    if (*t == NULL)
        return;
    for (i = 0; commandTable[i].command != NULL; i++)
        if (strcmp(commandTable[i].command, *t) == 0)
            break;

    rc = (*commandTable[i].handler)(t);    // call command handler
    if (rc != 0) {
        ReportExitStatus(rc);
        Printf("~\n");
    }
}
