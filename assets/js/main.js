document.addEventListener('DOMContentLoaded', () => {
  const navToggle = document.getElementById('navToggle');
  const navOverlay = document.getElementById('navOverlay');
  const navClose = document.getElementById('navClose');
  const scrollTopBtn = document.getElementById('scrollTop');
  const yearEl = document.getElementById('year');
  const yearNavEl = document.getElementById('yearNav');
  const contactForm = document.getElementById('contactForm');
  const formNote = document.getElementById('formNote');

  const year = new Date().getFullYear();
  if (yearEl) yearEl.textContent = year;
  if (yearNavEl) yearNavEl.textContent = year;

  // Scroll-to-top visibility
  const onScroll = () => {
    scrollTopBtn.classList.toggle('visible', window.scrollY > 500);
  };
  onScroll();
  window.addEventListener('scroll', onScroll, { passive: true });

  scrollTopBtn.addEventListener('click', () => {
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });

  // Full-screen nav overlay toggle
  const closeNav = () => {
    navOverlay.classList.remove('open');
    navOverlay.setAttribute('aria-hidden', 'true');
    navToggle.setAttribute('aria-expanded', 'false');
    navToggle.setAttribute('aria-label', 'Otwórz menu');
    document.body.style.overflow = '';
  };
  const openNav = () => {
    navOverlay.classList.add('open');
    navOverlay.setAttribute('aria-hidden', 'false');
    navToggle.setAttribute('aria-expanded', 'true');
    navToggle.setAttribute('aria-label', 'Zamknij menu');
    document.body.style.overflow = 'hidden';
  };

  navToggle.addEventListener('click', () => {
    const isOpen = navOverlay.classList.contains('open');
    isOpen ? closeNav() : openNav();
  });

  navClose.addEventListener('click', closeNav);

  navOverlay.querySelectorAll('a').forEach((link) => {
    link.addEventListener('click', closeNav);
  });

  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && navOverlay.classList.contains('open')) closeNav();
  });

  // Scroll reveal animations
  const revealEls = document.querySelectorAll('.reveal');
  if ('IntersectionObserver' in window) {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add('is-visible');
            observer.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.15, rootMargin: '0px 0px -40px 0px' }
    );
    revealEls.forEach((el) => observer.observe(el));
  } else {
    revealEls.forEach((el) => el.classList.add('is-visible'));
  }

  // Contact form (front-end only — no backend wired up yet)
  if (contactForm) {
    contactForm.addEventListener('submit', (e) => {
      e.preventDefault();
      if (!contactForm.checkValidity()) {
        contactForm.reportValidity();
        return;
      }
      formNote.textContent = 'Dziękujemy! Wiadomość została przygotowana do wysyłki (podłącz backend, aby ją faktycznie dostarczyć).';
      contactForm.reset();
    });
  }
});
