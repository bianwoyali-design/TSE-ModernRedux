/**
 * TSE Web Frontend — client-side search logic
 */

let currentQuery = '';
let currentPage = 1;
const RESULTS_PER_PAGE = 20;

document.addEventListener('DOMContentLoaded', () => {
  const form = document.getElementById('search-form');
  const input = document.getElementById('q');
  const statsEl = document.getElementById('stats');
  const resultsEl = document.getElementById('results');
  const paginationEl = document.getElementById('pagination');
  const overlay = document.getElementById('snapshot-overlay');
  const snapshotFrame = document.getElementById('snapshot-frame');
  const closeBtn = document.getElementById('snapshot-close');

  // Focus the search input
  input.focus();

  // Handle form submission
  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    const q = input.value.trim();
    if (!q) return;
    currentQuery = q;
    currentPage = 1;
    await doSearch(q, 1);
    // Update URL without reload
    const url = new URL(window.location);
    url.searchParams.set('q', q);
    window.history.pushState({}, '', url);
  });

  // Handle pagination clicks
  paginationEl.addEventListener('click', (e) => {
    if (e.target.classList.contains('page-link')) {
      e.preventDefault();
      const page = parseInt(e.target.dataset.page, 10);
      if (page > 0) {
        currentPage = page;
        doSearch(currentQuery, page);
        window.scrollTo({ top: 0, behavior: 'smooth' });
      }
    }
  });

  // Handle snapshot link clicks
  resultsEl.addEventListener('click', (e) => {
    if (e.target.classList.contains('snapshot-link')) {
      e.preventDefault();
      const url = e.target.dataset.url;
      openSnapshot(url);
    }
  });

  // Close snapshot overlay
  closeBtn.addEventListener('click', closeSnapshot);
  overlay.addEventListener('click', (e) => {
    if (e.target === overlay) closeSnapshot();
  });

  // Handle back/forward browser buttons
  window.addEventListener('popstate', () => {
    const params = new URLSearchParams(window.location.search);
    const q = params.get('q');
    if (q) {
      input.value = q;
      currentQuery = q;
      currentPage = 1;
      doSearch(q, 1);
    }
  });

  // Check for query in URL on initial load
  const params = new URLSearchParams(window.location.search);
  const q = params.get('q');
  if (q) {
    input.value = q;
    currentQuery = q;
    doSearch(q, 1);
  }
});

/**
 * Perform search via AJAX
 */
async function doSearch(query, page) {
  const statsEl = document.getElementById('stats');
  const resultsEl = document.getElementById('results');
  const paginationEl = document.getElementById('pagination');

  try {
    const resp = await fetch(`/api/search?q=${encodeURIComponent(query)}&p=${page}`);
    if (!resp.ok) {
      showError(`Server returned ${resp.status}`);
      return;
    }
    const data = await resp.json();

    // Show stats
    statsEl.classList.remove('hidden');
    const start = (page - 1) * RESULTS_PER_PAGE + 1;
    const end = Math.min(page * RESULTS_PER_PAGE, data.total);
    statsEl.textContent = `About ${data.total} results (${data.time_ms.toFixed(1)} ms)`;

    // Show results
    if (data.results && data.results.length > 0) {
      resultsEl.innerHTML = data.results.map((r, i) => renderResult(r, start + i)).join('');
    } else {
      resultsEl.innerHTML = '<div class="no-results">No results found for this query.</div>';
    }

    // Show pagination
    const totalPages = Math.ceil(data.total / RESULTS_PER_PAGE);
    if (totalPages > 1) {
      paginationEl.classList.remove('hidden');
      paginationEl.innerHTML = renderPagination(page, totalPages);
    } else {
      paginationEl.classList.add('hidden');
    }
  } catch (err) {
    showError(`Search failed: ${err.message}`);
  }
}

/**
 * Render a single search result item
 */
function renderResult(r, rank) {
  const url = escapeHtml(r.url || '');
  const title = escapeHtml(r.title || url || '(no title)');
  const snippet = r.snippet || '';
  const size = r.size ? ` &middot; ${formatSize(r.size)}` : '';

  return `
    <li class="result-item">
      <div class="result-url">${url}</div>
      <div class="result-title">
        <a href="${url}" target="_blank" rel="noopener">${title}</a>
      </div>
      <div class="result-snippet">${snippet}</div>
      <div class="result-meta">
        <a href="#" class="snapshot-link" data-url="${encodeURIComponent(r.url)}">Cached</a>
        ${size}
      </div>
    </li>`;
}

/**
 * Render pagination links
 */
function renderPagination(current, total) {
  let html = '';
  const maxShow = 10;
  const start = Math.max(1, current - Math.floor(maxShow / 2));
  const end = Math.min(total, start + maxShow - 1);

  if (current > 1) {
    html += `<a href="#" class="page-link" data-page="${current - 1}">&laquo; Prev</a>`;
  }

  for (let i = start; i <= end; i++) {
    if (i === current) {
      html += `<span class="current">${i}</span>`;
    } else {
      html += `<a href="#" class="page-link" data-page="${i}">${i}</a>`;
    }
  }

  if (current < total) {
    html += `<a href="#" class="page-link" data-page="${current + 1}">Next &raquo;</a>`;
  }

  return html;
}

/**
 * Open snapshot in overlay
 */
function openSnapshot(url) {
  const overlay = document.getElementById('snapshot-overlay');
  const frame = document.getElementById('snapshot-frame');
  frame.src = `/api/snapshot?url=${url}&word=${encodeURIComponent(currentQuery)}`;
  overlay.classList.remove('hidden');
  document.body.style.overflow = 'hidden';
}

/**
 * Close snapshot overlay
 */
function closeSnapshot() {
  const overlay = document.getElementById('snapshot-overlay');
  const frame = document.getElementById('snapshot-frame');
  overlay.classList.add('hidden');
  frame.src = '';
  document.body.style.overflow = '';
}

/**
 * Show error state
 */
function showError(msg) {
  document.getElementById('stats').classList.add('hidden');
  document.getElementById('results').innerHTML =
    `<div class="no-results">${escapeHtml(msg)}</div>`;
  document.getElementById('pagination').classList.add('hidden');
}

/**
 * Escape HTML entities
 */
function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

/**
 * Format file size
 */
function formatSize(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}
