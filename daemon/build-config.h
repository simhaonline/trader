#ifndef BUILDCONFIG_H
#define BUILDCONFIG_H

#define BUILD_VERSION "1.76u"

/// select your exchanges
#define BITTREX_ENABLED
#define BINANCE_ENABLED
#define POLONIEX_ENABLED
#define WAVES_ENABLED

/// universal exchange options
#define BITTREX_TICKER_ONLY
#define BINANCE_TICKER_ONLY
#define POLONIEX_TICKER_ONLY

/// where to print logs
#define PRINT_LOGS_TO_CONSOLE
//#define PRINT_LOGS_TO_FILE
#define PRINT_LOGS_TO_FILE_COLOR

/// to make trades with the strategy, comment this out
#define PAPER_TRADE

/// what to log
//#define PRINT_LOGS_WITH_FUNCTION_NAMES
#define PRINT_ENABLED_SSL_CIPHERS
//#define PRINT_DISABLED_SSL_CIPHERS

#endif // BUILDCONFIG_H
