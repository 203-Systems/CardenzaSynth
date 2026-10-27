// GET /api/search?q=音量&lang=zh — the Learn manual's search as a service (Vercel function). Same
// core and index as the page, so results match what the page shows.
const LearnSearch = require('../learn/learn-search.js');
const data = require('../learn/search-index.json');

const index = LearnSearch.createIndex(data);

module.exports = (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const query = (url.searchParams.get('q') || '').slice(0, 200);
  const lang = url.searchParams.get('lang') === 'en' ? 'en' : url.searchParams.get('lang') === 'all' ? '' : 'zh';
  const limit = Math.min(50, Math.max(1, parseInt(url.searchParams.get('limit') || '12', 10) || 12));
  const results = LearnSearch.search(index, query, { lang, limit });
  res.setHeader('Content-Type', 'application/json; charset=utf-8');
  res.setHeader('Cache-Control', 'public, max-age=300');
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.end(JSON.stringify({ version: data.version, build: data.build, query, lang: lang || 'all', results }));
};
