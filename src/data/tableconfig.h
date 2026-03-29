#ifndef TABLECONFIG_H
#define TABLECONFIG_H

/**
 * @brief Table column index configuration constants
 * 
 * This file contains configuration for all table column indices used throughout
 * the application to avoid hardcoded magic numbers.
 */

namespace TableConfig {

    /**
     * @brief Log table column indices (LogModel)
     */
    namespace LogColumns {
        constexpr int DATE    = 0;
        constexpr int TIME    = 1;
        constexpr int PID     = 2;
        constexpr int TID     = 3;
        constexpr int PACKAGE = 4;
        constexpr int LEVEL   = 5;
        constexpr int TAG     = 6;
        constexpr int MESSAGE = 7;
        constexpr int DELTA   = 8;   ///< Time delta column (mark table only)
        constexpr int TOTAL_COLUMNS = 8;   ///< LogModel column count (no DELTA)

        namespace Names {
            constexpr const char* DATE    = "Date";
            constexpr const char* TIME    = "Time";
            constexpr const char* PID     = "PID";
            constexpr const char* TID     = "TID";
            constexpr const char* PACKAGE = "Package";
            constexpr const char* LEVEL   = "Lvl";
            constexpr const char* TAG     = "Tag";
            constexpr const char* MESSAGE = "Message";
            constexpr const char* DELTA   = "\u0394Time";
        }
    }

    /**
     * @brief Settings table column indices (SettingsModel)
     */
    namespace SettingsColumns {
        constexpr int LINE    = 0;
        constexpr int GROUP   = 1;
        constexpr int SETTING = 2;
        constexpr int VALUE   = 3;
        constexpr int ACTION  = 4;
        constexpr int TOTAL_COLUMNS = 5;

        namespace Names {
            constexpr const char* LINE    = "LINE";
            constexpr const char* GROUP   = "GROUP";
            constexpr const char* SETTING = "SETTING";
            constexpr const char* VALUE   = "VALUE";
            constexpr const char* ACTION  = "";
        }
    }

    /**
     * @brief Properties table column indices (PropertiesModel)
     */
    namespace PropertiesColumns {
        constexpr int LINE     = 0;
        constexpr int PROPERTY = 1;
        constexpr int VALUE    = 2;
        constexpr int ACTION   = 3;
        constexpr int TOTAL_COLUMNS = 4;

        namespace Names {
            constexpr const char* LINE     = "LINE";
            constexpr const char* PROPERTY = "PROPERTY";
            constexpr const char* VALUE    = "VALUE";
            constexpr const char* ACTION   = "";
        }
    }

    /**
     * @brief Property Definition table column indices (PropertyDefinitionModel)
     */
    namespace PropertyDefColumns {
        constexpr int ID            = 0;
        constexpr int NAME          = 1;
        constexpr int SUPPORTED     = 2;
        constexpr int NEED_REBOOT   = 3;
        constexpr int TYPE          = 4;
        constexpr int READ_ONLY     = 5;
        constexpr int DEFAULT       = 6;
        constexpr int VALUE         = 7;
        constexpr int SET_BUTTON    = 8;
        constexpr int GET_BUTTON    = 9;
        constexpr int REMOVE_BUTTON = 10;
        constexpr int TOTAL_COLUMNS = 11;

        namespace Names {
            constexpr const char* ID            = "ID";
            constexpr const char* NAME          = "Name";
            constexpr const char* SUPPORTED     = "Supported";
            constexpr const char* NEED_REBOOT   = "Need Reboot";
            constexpr const char* TYPE          = "Type";
            constexpr const char* READ_ONLY     = "Read Only";
            constexpr const char* DEFAULT       = "Default";
            constexpr const char* VALUE         = "Value";
            constexpr const char* SET_BUTTON    = "Set";    ///< Label used in settings dialog; table header is empty for button columns
            constexpr const char* GET_BUTTON    = "Get";    ///< Label used in settings dialog; table header is empty for button columns
            constexpr const char* REMOVE_BUTTON = "Remove"; ///< Label used in settings dialog; table header is empty for button columns
        }
    }

    /**
     * @brief Column widths configuration
     */
    namespace ColumnWidths {
        // Log table widths
        constexpr int LOG_DATE = 100;
        constexpr int LOG_TIME = 120;
        constexpr int LOG_PID = 60;
        constexpr int LOG_TID = 60;
        constexpr int LOG_PACKAGE = 200;
        constexpr int LOG_LEVEL = 35;
        constexpr int LOG_DELTA = 80;

        // Settings table widths
        constexpr int SETTINGS_LINE = 50;
        constexpr int SETTINGS_GROUP = 150;
        constexpr int SETTINGS_SETTING = 250;
        constexpr int SETTINGS_VALUE = 300;
        constexpr int SETTINGS_ACTION = 60;

        // Properties table widths
        constexpr int PROPERTIES_LINE = 50;
        constexpr int PROPERTIES_PROPERTY = 250;
        constexpr int PROPERTIES_VALUE = 300;
        constexpr int PROPERTIES_ACTION = 60;

        // Property Definition table widths
        constexpr int PROPDEF_NAME = 200;
        constexpr int PROPDEF_ID = 80;
        constexpr int PROPDEF_SUPPORTED = 90;
        constexpr int PROPDEF_DEFAULT = 150;
        constexpr int PROPDEF_NEED_REBOOT = 100;
        constexpr int PROPDEF_TYPE = 100;
        constexpr int PROPDEF_READ_ONLY = 90;
        constexpr int PROPDEF_SET_BUTTON = 34;
        constexpr int PROPDEF_GET_BUTTON = 34;
        constexpr int PROPDEF_REMOVE_BUTTON = 34;
    }
}

#endif // TABLECONFIG_H
