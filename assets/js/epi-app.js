/**
 * EPI app prototype — runs the detection state machine from the design document
 * (CONFIRMING -> ALARM, or ANULOWANO when the user cancels in time) on the
 * middle phone screen of the projects page.
 *
 * Wording lives in data attributes on the markup so each language version
 * carries its own; this file only drives the states.
 */
document.addEventListener('DOMContentLoaded', () => {
  const screen = document.getElementById('epiAlarmScreen');
  const replay = document.getElementById('epiReplay');
  if (!screen || !replay) return;

  const stateEl = document.getElementById('epiAlarmState');
  const titleEl = document.getElementById('epiAlarmTitle');
  const countWrap = document.getElementById('epiCountdownWrap');
  const countEl = document.getElementById('epiCountdown');
  const triggers = document.getElementById('epiTriggers');
  const cancelBtn = document.getElementById('epiCancel');
  const notifyEl = document.getElementById('epiNotify');

  const t = screen.dataset;
  const START = 15;
  let remaining = START;
  let timer = null;

  const clear = () => {
    if (timer) { clearInterval(timer); timer = null; }
  };

  const setResolved = (state, title, note) => {
    clear();
    stateEl.textContent = state;
    titleEl.textContent = title;
    countWrap.hidden = true;
    triggers.hidden = true;
    cancelBtn.hidden = true;
    notifyEl.textContent = note;
    replay.disabled = false;
    replay.firstChild.textContent = t.restart + ' ';
  };

  const reset = () => {
    clear();
    remaining = START;
    screen.classList.add('is-alarm');
    stateEl.textContent = t.stateConfirming;
    titleEl.textContent = t.titleConfirming;
    countEl.textContent = START;
    countWrap.hidden = false;
    triggers.hidden = false;
    cancelBtn.hidden = false;
    notifyEl.textContent = t.notify;
  };

  const tick = () => {
    remaining -= 1;
    countEl.textContent = remaining;
    if (remaining <= 0) {
      setResolved(t.stateSent, t.titleSent, t.noteSent);
    }
  };

  const run = () => {
    reset();
    replay.disabled = true;
    // Honour reduced motion by resolving immediately rather than counting down.
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      setResolved(t.stateSent, t.titleSent, t.noteSent);
      return;
    }
    timer = setInterval(tick, 1000);
  };

  cancelBtn.addEventListener('click', () => {
    setResolved(t.stateCancelled, t.titleCancelled, t.noteCancelled);
  });

  replay.addEventListener('click', run);
});
