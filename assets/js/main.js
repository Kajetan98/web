document.addEventListener('DOMContentLoaded', () => {
  const header = document.getElementById('siteHeader');
  const navToggle = document.getElementById('navToggle');
  const mainNav = document.getElementById('mainNav');
  const scrollTopBtn = document.getElementById('scrollTop');
  const yearEl = document.getElementById('year');
  const contactForm = document.getElementById('contactForm');
  const formNote = document.getElementById('formNote');

  if (yearEl) yearEl.textContent = new Date().getFullYear();

  // Sticky header background on scroll + scroll-to-top visibility
  const onScroll = () => {
    const scrolled = window.scrollY > 12;
    header.classList.toggle('scrolled', scrolled);
    scrollTopBtn.classList.toggle('visible', window.scrollY > 500);
  };
  onScroll();
  window.addEventListener('scroll', onScroll, { passive: true });

  scrollTopBtn.addEventListener('click', () => {
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });

  // Mobile nav toggle
  navToggle.addEventListener('click', () => {
    const isOpen = mainNav.classList.toggle('open');
    navToggle.setAttribute('aria-expanded', String(isOpen));
    navToggle.setAttribute('aria-label', isOpen ? 'Zamknij menu' : 'Otwórz menu');
    document.body.style.overflow = isOpen ? 'hidden' : '';
  });

  mainNav.querySelectorAll('a').forEach((link) => {
    link.addEventListener('click', () => {
      mainNav.classList.remove('open');
      navToggle.setAttribute('aria-expanded', 'false');
      navToggle.setAttribute('aria-label', 'Otwórz menu');
      document.body.style.overflow = '';
    });
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
