// HeaterMeter Copyright 2016 Bryan Mayland <bmayland@capnbry.net>
#include "Arduino.h"
#include <avr/pgmspace.h>
#include "menus.h"

MenuSystem::MenuSystem(const menu_definition_t *defs, const menu_transition_t *trans,
  const buttonread_t reader)
  : m_definitions(defs), m_transitions(trans), m_readButton(reader)
  // State(ST_NONE), m_lastButton(BUTTON_NONE)
{
}

inline unsigned long MenuSystem::getTimeoutDuration(void) const
{
  return (m_currMenu) ? (unsigned long)pgm_read_byte(&m_currMenu->timeout) * 1000 : 0;
}

inline handler_t MenuSystem::getHandler(void) const
{
  return (m_currMenu) ? (handler_t)pgm_read_word(&m_currMenu->handler) : 0;
}

inline unsigned long MenuSystem::getElapsedDuration(void) const
{
  return millis() - m_lastActivity;
}

inline state_t MenuSystem::findTransition(button_t button) const
{
  const menu_transition_t *trans = m_transitions;
  state_t lookup;
  while ((lookup = pgm_read_byte(&trans->state)))
  {
    if (lookup == m_state)
    {
      button_t transButton = pgm_read_byte(&trans->button);
      if ((button & transButton) == button)
        return pgm_read_byte(&trans->newstate);
    }
    ++trans;
  }
  return m_state;
}

void MenuSystem::setState(state_t state)
{
  //Serial.print("$HMLG,Setting state: ");
  //Serial.println(state, DEC);
  m_lastState = m_state;

  while (state > ST_VMAX && state != m_state)
  {
    handler_t handler = getHandler();
    if (handler)
      handler(BUTTON_LEAVE);

    m_state = state;
    m_currMenu = m_definitions;

    state_t lookup;
    while ((lookup = pgm_read_byte(&m_currMenu->state)))
    {
      if (lookup == m_state)
        break;
      ++m_currMenu;
    }
    
    if (m_currMenu)
    {
      handler = getHandler();
      if (handler)
        state = handler(BUTTON_ENTER);
    } else {
      state = ST_NONE;
    }
  }  // while state changing

  m_lastActivity = millis();
}

void MenuSystem::doWork(void)
{
  unsigned long elapsed = getElapsedDuration();
  // Operate button loop at 4Hz unless no button has been pressed in one cycle
  if (elapsed < 250)
    return;

  button_t button = m_readButton();
  if (button == BUTTON_NONE)
  {
    unsigned long dur = getTimeoutDuration();
    if (dur != 0 && dur < elapsed)
      button = BUTTON_TIMEOUT;
  }

  if (button != BUTTON_TIMEOUT)
  {
    if (button == m_lastButton)
    {
      if (m_buttonRepeatCnt < 0xff)
        ++m_buttonRepeatCnt;
    }
    else
    {
      m_lastButton = button;
      m_buttonRepeatCnt = 0;
    }
  }

  if (button == BUTTON_NONE)
    return;

  //Serial.print("New button: ");
  //Serial.println(button, DEC);

  m_lastActivity = millis();
  
  state_t newState = ST_AUTO;
  handler_t handler = getHandler();
  if (handler != NULL)
    newState = handler(button);
  if (newState == ST_AUTO)
    newState = findTransition(button);

  setState(newState);
}
