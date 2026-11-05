// One page per guide lesson, /guide/<lesson>/ (the guide's home is src/pages/index.tsx), in every
// locale. The lessons are generated/data/guide.json (tools/wiki/generate.py, from docs/learn/guide.json).
const fs = require('fs');
const path = require('path');

module.exports = function guideRoutes(context) {
  const file = path.join(context.siteDir, 'generated', 'data', 'guide.json');
  return {
    name: 'cardenza-guide-routes',
    getPathsToWatch() {
      return [file];
    },
    async contentLoaded({actions}) {
      const guide = JSON.parse(fs.readFileSync(file, 'utf8'));
      for (const id of guide.order) {
        actions.addRoute({
          path: `${context.baseUrl}guide/${id}/`,
          component: '@site/src/guide/LessonPage',
          exact: true,
          props: {lessonId: id},
        });
      }
    },
  };
};
