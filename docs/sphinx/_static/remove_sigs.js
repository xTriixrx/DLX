const anchorMap = {};
const structData = {};

const moduleLinks = [
  { href: 'index.html', label: 'Overview' },
  { href: 'core.html', label: 'Core Modules' },
  { href: 'sudoku.html', label: 'Sudoku Modules' },
  { href: 'tools.html', label: 'Tooling & Extensions' },
  { href: 'reference.html', label: 'Full API Reference' },
];

const cleanText = (node) => {
  if (!node) {
    return '';
  }
  const clone = node.cloneNode(true);
  return clone.textContent.trim().replace(/\s+/g, ' ');
};

const cloneWithoutHeaderLinks = (node) => {
  if (!node) {
    return null;
  }
  const clone = node.cloneNode(true);
  if (clone.querySelectorAll) {
    clone.querySelectorAll('.headerlink').forEach((link) => link.remove());
    clone.querySelectorAll('br').forEach((br) => br.remove());
  }
  return clone;
};

const collectAnchorIds = (root) => {
  if (!root) {
    return [];
  }
  const ids = [];
  const seen = new Set();
  const addId = (value) => {
    if (value && !seen.has(value)) {
      seen.add(value);
      ids.push(value);
    }
  };
  addId(root.id);
  root.querySelectorAll('[id]').forEach((el) => addId(el.id));
  return ids;
};

const findPrototypeBlock = (section) => {
  const nestedBlock = section.querySelector('.astro-mui-prototypes');
  if (nestedBlock) {
    return nestedBlock;
  }

  let current = section;
  while (current) {
    let sibling = current.previousElementSibling;
    while (sibling) {
      if (sibling.classList && sibling.classList.contains('astro-mui-prototypes')) {
        return sibling;
      }
      sibling = sibling.previousElementSibling;
    }
    current = current.parentElement;
  }
  return null;
};

const enhanceReferencePrototypes = () => {
  if (!/reference\.html$/.test(window.location.pathname)) {
    return;
  }

  const cleanSignatureText = (node) => {
    const clone = cloneWithoutHeaderLinks(node);
    if (!clone) {
      return '';
    }
    return clone.textContent.replace(/\s+/g, ' ').trim();
  };

  const cleanSignatureHtml = (node) => {
    const clone = cloneWithoutHeaderLinks(node);
    if (!clone) {
      return '';
    }
    return clone.innerHTML.trim();
  };

  const ensureSemicolon = (snippet) => {
    const trimmed = snippet.trim();
    if (!trimmed) {
      return snippet;
    }
    const needsSemicolon = /(\)|\w)\s*$/.test(trimmed) && !/[{;]$/.test(trimmed);
    return needsSemicolon ? `${trimmed};` : trimmed;
  };

  const createPrototypeBlock = (html) => {
    const container = document.createElement('div');
    container.className = 'astro-mui-prototypes highlight-cpp notranslate';
    container.dataset.referenceProto = 'true';

    const highlight = document.createElement('div');
    highlight.className = 'highlight';

    const pre = document.createElement('pre');
    pre.innerHTML = html;
    pre.style.whiteSpace = 'pre';

    highlight.appendChild(pre);
    container.appendChild(highlight);
    return container;
  };

  const selectors = ['.cpp.function', '.cpp.struct', '.cpp.type', '.cpp.macro'];
  document.querySelectorAll(selectors.join(', ')).forEach((node) => {
    const signature = node.querySelector('.sig.sig-object.cpp');
    const sigNode = signature || node;
    const plain = cleanSignatureText(sigNode);
    const html = cleanSignatureHtml(sigNode);
    if (!plain || !html) {
      return;
    }

    const needsSemicolon = /(\)|\w)\s*$/.test(plain) && !/[{;]$/.test(plain);
    const htmlContent = `<span></span>${html}${needsSemicolon ? '<span class=\"p\">;</span>' : ''}`;

    const findExisting = () => {
      let next = node.nextElementSibling;
      while (next && next.classList && next.classList.contains('astro-anchor-target')) {
        next = next.nextElementSibling;
      }
      if (next && next.dataset && next.dataset.referenceProto === 'true') {
        return next;
      }
      return null;
    };

    const insertionTarget = () => {
      let ref = node;
      let next = node.nextElementSibling;
      while (next && next.classList && next.classList.contains('astro-anchor-target')) {
        ref = next;
        next = next.nextElementSibling;
      }
      return ref;
    };

    const existing = findExisting();
    if (existing) {
      const pre = existing.querySelector('pre');
      if (pre) {
        pre.innerHTML = htmlContent;
        pre.style.whiteSpace = 'pre';
      }
      return;
    }

    const block = createPrototypeBlock(htmlContent);
    insertionTarget().insertAdjacentElement('afterend', block);
  });
};

const injectPrimaryNavMenu = () => {
  if (document.querySelector('.astro-primary-modules')) {
    return;
  }

  const primaryNav =
    document.querySelector('.md-sidebar.md-sidebar--primary .md-nav') ||
    document.querySelector('.sphinxsidebar .sphinxsidebarwrapper');

  if (!primaryNav) {
    return;
  }

  const container = document.createElement('div');
  container.className = 'astro-primary-modules';

  const title = document.createElement('div');
  title.className = 'astro-primary-modules-title';
  title.textContent = 'Module Navigation';
  container.appendChild(title);

  const list = document.createElement('ul');
  list.className = 'astro-primary-modules-list';

  moduleLinks.forEach((entry) => {
    if (!entry.href || !entry.label) {
      return;
    }
    const item = document.createElement('li');
    item.className = 'astro-primary-modules-item';

    const link = document.createElement('a');
    link.href = entry.href;
    link.textContent = entry.label;
    link.className = 'astro-primary-modules-link';

    item.appendChild(link);
    list.appendChild(item);
  });

  container.appendChild(list);
  primaryNav.insertBefore(container, primaryNav.firstChild);
};

document.addEventListener('DOMContentLoaded', () => {
  const signatureSelectors = ['.cpp.function', '.cpp.struct', '.cpp.macro', '.cpp.type'];

  document.querySelectorAll(signatureSelectors.join(', ')).forEach((section) => {
    const dd = section.querySelector('dd');
    const block = findPrototypeBlock(section);

    if (block && dd && block.parentElement !== dd) {
      dd.insertBefore(block, dd.firstChild);
    }

    const sig = section.querySelector('.sig.sig-object.cpp');
    if (sig) {
      const ids = collectAnchorIds(sig);

      if (ids.length > 0) {
        const primaryId = ids.shift();
        const allIds = [primaryId, ...ids];

        if (section.classList.contains('struct')) {
          const structId = primaryId;
          const structName = cleanText(sig.querySelector('.sig-name')) || structId;
          section.dataset.astroStructId = structId;
          if (!structData[structId]) {
            structData[structId] = {
              name: structName.trim(),
              groups: { public: [], private: [] },
            };
          }
        }

        allIds.forEach((id) => {
          anchorMap[id] = primaryId;
        });

        const fragment = document.createDocumentFragment();

        allIds.forEach((id) => {
          let anchor = document.getElementById(id);
          if (anchor && (anchor === sig || sig.contains(anchor))) {
            anchor = null;
          }

          if (!anchor) {
            anchor = document.createElement('a');
            anchor.id = id;
            anchor.className = 'astro-anchor-target';
            fragment.appendChild(anchor);
          }
        });

        if (fragment.childNodes.length > 0) {
          if (section.parentNode) {
            section.parentNode.insertBefore(fragment, section);
          } else if (block && block.parentNode) {
            block.parentNode.insertBefore(fragment, block);
          } else if (dd) {
            dd.insertBefore(fragment, dd.firstChild);
          }
        }
      }

      sig.style.display = 'none';
    }
  });

  document
    .querySelectorAll('.md-nav__link[href^="#"], .md-sidebar.md-sidebar--secondary .md-nav.md-nav--secondary a[href^="#"]')
    .forEach((link) => {
      const target = link.getAttribute('href').slice(1);
      if (anchorMap[target]) {
        link.setAttribute('href', `#${anchorMap[target]}`);
      }
    });

  const secondaryNavLinks = new Map();
  document
    .querySelectorAll('.md-sidebar.md-sidebar--secondary .md-nav.md-nav--secondary a[href^="#"]')
    .forEach((link) => {
      const target = link.getAttribute('href').slice(1);
      secondaryNavLinks.set(target, link);
    });

  document.querySelectorAll('.cpp.function dd p').forEach((p) => {
    p.classList.add('astro-doc-text');
  });

  document.querySelectorAll('.cpp.function dd dl.field-list').forEach((dl) => {
    const container = document.createElement('div');
    container.className = 'astro-field-list';

    let node = dl.firstElementChild;
    while (node) {
      if (node.tagName === 'DT') {
        const titleText = cleanText(node);
        let descNode = node.nextElementSibling;
        let descText = '';
        if (descNode && descNode.tagName === 'DD') {
          descText = descNode.textContent.trim();
        }

        const field = document.createElement('div');
        field.className = 'astro-field';

        const title = document.createElement('div');
        title.className = 'astro-field-title';
        title.textContent = titleText;

        const desc = document.createElement('p');
        desc.className = 'astro-field-desc';
        desc.textContent = descText;

        field.appendChild(title);
        field.appendChild(desc);
        container.appendChild(field);
      }
      node = node.nextElementSibling;
    }

    dl.replaceWith(container);
  });

  document.querySelectorAll('.breathe-sectiondef-title.rubric').forEach((title) => {
    const heading = title.textContent.trim().toLowerCase();
    if (heading === 'public members' || heading === 'private members') {
      const wrapper = title.parentElement;
      if (!wrapper) {
        return;
      }

      const members = wrapper.querySelectorAll('dl.cpp.var');
      if (members.length === 0) {
        return;
      }

      const fieldList = document.createElement('div');
      fieldList.className = 'astro-field-list';

      members.forEach((dl) => {
        const dt = dl.querySelector('dt');
        const dd = dl.querySelector('dd');

        const field = document.createElement('div');
        field.className = 'astro-field astro-field-member';

        const titleEl = document.createElement('div');
        titleEl.className = 'astro-field-title';
        titleEl.textContent = cleanText(dt);

        field.appendChild(titleEl);

        const ids = collectAnchorIds(dt);

        const descText = dd ? dd.textContent.trim() : '';
        if (descText.length > 0) {
          const descEl = document.createElement('p');
          descEl.className = 'astro-field-desc astro-field-member-desc';
          descEl.textContent = descText;
          field.appendChild(descEl);
        }

        const anchorFragment = document.createDocumentFragment();
        if (ids.length > 0) {
          const primaryId = ids[0];
          ids.forEach((id) => {
            anchorMap[id] = primaryId;
            let anchor = document.getElementById(id);
            if (anchor && (dt.contains(anchor) || field.contains(anchor))) {
              anchor = null;
            }
            if (!anchor) {
              anchor = document.createElement('a');
              anchor.id = id;
              anchor.className = 'astro-anchor-target';
              anchorFragment.appendChild(anchor);
            }
          });
        }

        if (anchorFragment.childNodes.length > 0) {
          fieldList.appendChild(anchorFragment);
        }

        fieldList.appendChild(field);

        const structSection = title.closest('.cpp.struct');
        const structId = structSection && structSection.dataset.astroStructId;
        if (structId && structData[structId]) {
          const groupKey = heading.includes('private') ? 'private' : 'public';
          const primaryId = ids[0];
          if (primaryId) {
            structData[structId].groups[groupKey].push({
              id: primaryId,
              text: titleEl.textContent,
            });
          }
        }
        dl.remove();
      });

      title.insertAdjacentElement('afterend', fieldList);
    }
  });

  Object.keys(structData).forEach((structId) => {
    const structLink = secondaryNavLinks.get(structId);
    if (!structLink) {
      return;
    }

    const structItem = structLink.closest('.md-nav__item') || structLink.parentElement;
    if (!structItem) {
      return;
    }

    const dropdownContainer = document.createElement('div');
    dropdownContainer.className = 'astro-nav-dropdown';

    ['public', 'private'].forEach((groupKey) => {
      const entries = structData[structId].groups[groupKey];
      if (!entries || entries.length === 0) {
        return;
      }

      const details = document.createElement('details');
      details.className = 'astro-nav-dropdown-group';

      const summary = document.createElement('summary');
      summary.className = 'astro-nav-dropdown-summary';
      summary.textContent = groupKey === 'public' ? 'Public Members' : 'Private Members';
      details.appendChild(summary);

      const list = document.createElement('ul');
      list.className = 'astro-nav-dropdown-list';

      entries.forEach((entry) => {
        const li = document.createElement('li');
        li.className = 'astro-nav-dropdown-item';

        const link = document.createElement('a');
        link.href = `#${entry.id}`;
        link.textContent = entry.text;
        link.className = 'astro-nav-dropdown-link';
        li.appendChild(link);
        list.appendChild(li);

        const existingLink = secondaryNavLinks.get(entry.id);
        if (existingLink) {
          const existingItem = existingLink.closest('.md-nav__item');
          if (existingItem) {
            existingItem.style.display = 'none';
          } else {
            existingLink.style.display = 'none';
          }
        }
      });

      details.appendChild(list);
      dropdownContainer.appendChild(details);
    });

    if (dropdownContainer.childNodes.length > 0) {
      structItem.insertAdjacentElement('afterend', dropdownContainer);
    }
  });

  injectPrimaryNavMenu();
  enhanceReferencePrototypes();
});
