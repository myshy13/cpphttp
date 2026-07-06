document.addEventListener('DOMContentLoaded', () => {
  const links = document.querySelectorAll('nav a[data-section]');
  const sections = document.querySelectorAll('main section');

  function activateSection(id) {
    sections.forEach(sec => sec.classList.remove('active'));
    links.forEach(l => l.classList.remove('active'));
    const target = document.getElementById(id);
    if (target) target.classList.add('active');
    const link = document.querySelector(`nav a[data-section="${id}"]`);
    if (link) link.classList.add('active');
  }

  // Activate first section by default
  if (sections.length) {
    const firstId = sections[0].id;
    activateSection(firstId);
  }

  links.forEach(link => {
    link.addEventListener('click', e => {
      e.preventDefault();
      const id = link.dataset.section;
      if (id) activateSection(id);
    });
  });
});