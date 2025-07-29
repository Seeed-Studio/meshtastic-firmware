#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "./GPIOJoystick.h"

#include "main.h"
#include "sleep.h"

using namespace NicheGraphics::Inputs;

GPIOJoystick::GPIOJoystick() : concurrency::OSThread("GPIOJoystick")
{
    // Don't start our timer
    // We only enable it from the ISRs, to run callbacks at next loop()
    OSThread::disable();

#ifdef ARCH_ESP32
    // Register callbacks for before and after lightsleep
    lsObserver.observe(&notifyLightSleep);
    lsEndObserver.observe(&notifyLightSleepEnd);
#endif
}

// Get access to (or create) the singleton instance of this class
// Accessible inside the ISRs, even though we maybe shouldn't
GPIOJoystick *GPIOJoystick::getInstance()
{
    // Instantiate the class the first time this method is called
    static GPIOJoystick *const singletonInstance = new GPIOJoystick;

    return singletonInstance;
}

// Set whether input is LOW or HIGH when pressed
// Set whether input uses the internal pulldown / pullup resistors
// Value applies to all of the joystick's inputs
// Need to store this info, as we detach and reattach inputs during light sleep (ESP32)
void GPIOJoystick::setWiringType(WiringType wiringType)
{
    wiring = wiringType; // Store as private member
}

void GPIOJoystick::setDebounce(uint32_t milliseconds)
{
    debounceMs = milliseconds; // Store as a private member
}

void GPIOJoystick::setPin(Direction direction, uint8_t pin)
{
    pins[direction] = pin; // Store as private member
}

// Specify which code to execute when joystick input happens
// This callback code should be a lambda function in setupNicheGraphics()
// The callback will not be run directly inside the ISR. It will be run "ASAP", at next loop()
void GPIOJoystick::setHandler(Direction direction, Callback onPress)
{
    callbacks[direction] = onPress; // Store as private member
}

// Begin receiving button input
// Attaches interrupts
// Do this after after configuring input, in setupNicheGraphics()
// (Also used internally after ESP32 light sleep)
void GPIOJoystick::start()
{
    assert(wiring != WiringType::UNSET);

    // Decide whether a rising or falling edge indicates button press
    uint32_t edge;
    if (wiring == WiringType::ACTIVE_HIGH || wiring == WiringType::ACTIVE_HIGH_PULLDOWN)
        edge = RISING;
    else
        edge = FALLING;

    if (pins[Direction::UP] != 0xFF)
        attachInterrupt(pins[Direction::UP], GPIOJoystick::isrUP, edge);

    if (pins[Direction::RIGHT] != 0xFF)
        attachInterrupt(pins[Direction::RIGHT], GPIOJoystick::isrRIGHT, edge);

    if (pins[Direction::DOWN] != 0xFF)
        attachInterrupt(pins[Direction::DOWN], GPIOJoystick::isrDOWN, edge);

    if (pins[Direction::LEFT] != 0xFF)
        attachInterrupt(pins[Direction::LEFT], GPIOJoystick::isrLEFT, edge);

    if (pins[Direction::CENTER] != 0xFF)
        attachInterrupt(pins[Direction::CENTER], GPIOJoystick::isrCENTER, edge);
}

// Stop receiving input
// Handles ESP32 light sleep
void GPIOJoystick::stop()
{
    if (pins[Direction::UP] != 0xFF)
        detachInterrupt(pins[Direction::UP]);

    if (pins[Direction::LEFT] != 0xFF)
        detachInterrupt(pins[Direction::LEFT]);

    if (pins[Direction::DOWN] != 0xFF)
        detachInterrupt(pins[Direction::DOWN]);

    if (pins[Direction::RIGHT] != 0xFF)
        detachInterrupt(pins[Direction::RIGHT]);

    if (pins[Direction::CENTER] != 0xFF)
        detachInterrupt(pins[Direction::CENTER]);
}

// Called from a pin's interrupt, when a new press is detected
// Starts the timer thread, which will poll for pin's release
void GPIOJoystick::handlePressBegin(Direction direction)
{
    // Ignore if still polling for a previous release.
    // Prevents button bounces during release, which would otherwise reset pressedAtMs
    if (OSThread::enabled)
        return;

    pressedAtMs = millis();           // For debouncing
    pressedDirection = direction;     // Store for use by runOnce
    OSThread::setIntervalFromNow(10); // Poll every 10ms for release
    OSThread::enabled = true;
    runASAP = true;
}

// Timer method
// Enabled by a pin's ISR when press starts
// Polls for release, and executes callback if held for longer than the debounce threshold
int32_t GPIOJoystick::runOnce()
{
    // Check if the button is still pressed
    bool isPressed;
    if (wiring == WiringType::ACTIVE_HIGH || wiring == WiringType::ACTIVE_HIGH_PULLDOWN)
        isPressed = (digitalRead(pins[pressedDirection]) == HIGH);
    else
        isPressed = (digitalRead(pins[pressedDirection]) == LOW);

    // If still pressed, check again in a few milliseconds
    if (isPressed)
        return OSThread::interval;

    // Otherwise, released now

    // If press was longer than the debounce threshold, execute the callback
    uint32_t duration = millis() - pressedAtMs;
    if (duration > debounceMs)
        callbacks[pressedDirection]();

    return OSThread::disable(); // Handled. Stop thread
}

#ifdef ARCH_ESP32

// Detach our class' interrupts before lightsleep
// Allows sleep.cpp to configure its own interrupts, which wake the device on user-button press
int TwoButton::beforeLightSleep(void *unused)
{
    stop();
    return 0; // Indicates success
}

// Reconfigure our interrupts
// Our class' interrupts were disconnected during sleep, to allow the user button to wake the device from sleep
int TwoButton::afterLightSleep(esp_sleep_wakeup_cause_t cause)
{
    start();
    return 0; // Indicates success
}

#endif

#endif