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
     * @brief SDK property table column indices (PropertyDefinitionModel)
     */
    namespace PropertyDefColumns {
        constexpr int NAME    = 0;
        constexpr int VALUE   = 1;
        constexpr int DEFAULT = 2;
        constexpr int TYPE    = 3;
        constexpr int ID      = 4;
        constexpr int NOTES   = 5;
        constexpr int TOTAL_COLUMNS = 6;

        namespace Names {
            constexpr const char* NAME    = "Name";
            constexpr const char* VALUE   = "Value";
            constexpr const char* DEFAULT = "Default";
            constexpr const char* TYPE    = "Type";
            constexpr const char* ID      = "ID";
            constexpr const char* NOTES   = "Notes";
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

        // SDK property table widths (Value stretches)
        constexpr int PROPDEF_NAME = 320;
        constexpr int PROPDEF_DEFAULT = 140;
        constexpr int PROPDEF_TYPE = 190;
        constexpr int PROPDEF_ID = 80;
        constexpr int PROPDEF_NOTES = 150;
    }
}

#endif // TABLECONFIG_H
