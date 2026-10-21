/**
 * @file main.cpp
 * @brief Arduino entry points; delegate to the App singleton.
 */

#include "app/App.h"

/**
 * @brief Arduino setup hook: boot the application once via App::begin().
 */
void setup() {
    App::instance().begin();
}

/**
 * @brief Arduino loop hook: run one App::loop() iteration (called repeatedly by the Arduino loop task).
 */
void loop() {
    App::instance().loop();
}
