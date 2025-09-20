
(() => {
  const target = 'arcware.html' + location.search + location.hash;
  // replace 不留 history 记录
  window.location.replace(target);
})();
