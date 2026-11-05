// The old manual's search regression (tools/learn/search.test.js: everyday questions in both
// languages must find their section among the first three, every title its own section), run on
// the wiki's index:  node scripts/search-test.js build
const fs = require('fs');
const path = require('path');
const {execFileSync} = require('child_process');

const build = path.resolve(process.argv[2] || 'build');
const tools = path.resolve(__dirname, '..', '..', 'tools', 'learn');
const dir = path.join(build, '.search-check');
fs.mkdirSync(dir, {recursive: true});
fs.copyFileSync(path.join(tools, 'learn-search.js'), path.join(dir, 'learn-search.js'));
fs.copyFileSync(path.join(build, 'search-index.json'), path.join(dir, 'search-index.json'));
try {
  execFileSync(process.execPath, [path.join(tools, 'search.test.js'), dir], {stdio: 'inherit'});
} finally {
  fs.rmSync(dir, {recursive: true, force: true});
}
