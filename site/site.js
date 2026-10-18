// The page's motion (anime.js 4, site/vendor): the hero's secant turning into the tangent, the
// tour's calculator following the chapters, sections revealed as they scroll in. With reduced
// motion everything is shown as it ends, still.
import { animate, createTimeline, stagger, svg, splitText } from './vendor/anime.esm.min.js';

window.deltaAnim = true;
const $ = s => document.querySelector(s);
const $$ = s => [...document.querySelectorAll(s)];
const still = matchMedia('(prefers-reduced-motion: reduce)').matches;

// ------------------------------------------------------------------ the secant (hero)
// f(x) = 2 + 1.6 sin(0.55x - 0.9) on [0, 10] x [-0.3, 4.2]; P at x0, Q at x0 + h
const f = x => 2 + 1.6 * Math.sin(0.55 * x - 0.9);
const X0 = 2.4, H0 = 3.6;
const px = x => 60 + (x / 10) * 550;
const py = y => 390 - ((y + 0.3) / 4.5) * 360;
const fig = $('.secant');
const set = (id, attrs) => { const e = document.getElementById(id); for (const k in attrs) e.setAttribute(k, attrs[k]); };

let d = '';
for (let i = 0; i <= 140; i++) {
  const x = (i / 140) * 10;
  d += (i ? 'L' : 'M') + px(x).toFixed(1) + ' ' + py(f(x)).toFixed(1);
}
set('curve', { d });
set('dot-p', { cx: px(X0), cy: py(f(X0)) });
set('pulse', { cx: px(X0), cy: py(f(X0)) });

const state = { h: H0 };
function render() {
  const h = Math.max(state.h, 1e-4), xq = X0 + h, y0 = f(X0), yq = f(xq), m = (yq - y0) / h;
  const xa = X0 - 3, xb = xq + 3;
  set('sec-line', { x1: px(xa), y1: py(y0 + m * (xa - X0)), x2: px(xb), y2: py(y0 + m * (xb - X0)) });
  set('leg-x', { x1: px(X0), y1: py(y0), x2: px(xq), y2: py(y0) });
  set('leg-y', { x1: px(xq), y1: py(y0), x2: px(xq), y2: py(yq) });
  set('dot-q', { cx: px(xq), cy: py(yq) });
  const show = Math.min(1, h / 1.1);
  set('lab-x', { x: (px(X0) + px(xq)) / 2 - 12, y: py(y0) + 28, opacity: show });
  set('lab-y', { x: px(xq) + 10, y: (py(y0) + py(yq)) / 2 + 7, opacity: show });
  set('leg-x', { opacity: show }); set('leg-y', { opacity: show });
  $('#ro-val').textContent = m.toFixed(3);
}

// ------------------------------------------------------------------ reveals
function rise(targets, delay = 0) {
  return animate(targets, { opacity: [0, 1], translateY: [26, 0], duration: 900, delay, ease: 'outExpo' });
}
function revealGroup(el) { // a list: the list at once, its items one after the other
  el.style.opacity = 1;
  animate(el.children, { opacity: [0, 1], translateY: [18, 0], duration: 700, delay: stagger(45), ease: 'outExpo' });
}

if (still) {
  $$('[data-reveal]').forEach(e => { e.style.opacity = 1; });
  state.h = 1e-4; render();
  fig.classList.add('locked');
  set('secant-group', { style: 'opacity:1' });
} else {
  // the hero, in order: the name a letter at a time, the rest, the curve drawn, then the secant
  const mark = $('#mark');
  mark.style.opacity = 1;
  const { chars } = splitText(mark, { chars: true });
  animate(chars, { opacity: [0, 1], translateY: ['0.45em', 0], rotate: [6, 0], duration: 900, delay: stagger(55, { start: 150 }), ease: 'outExpo' });
  $$('.hero-copy [data-reveal]:not(#mark)').forEach((e, i) => rise(e, 450 + i * 110));
  rise(fig, 300);
  const [curve] = svg.createDrawable('#curve');
  render();
  createTimeline()
    .add(curve, { draw: ['0 0', '0 1'], duration: 1700, ease: 'inOutQuad' }, 500)
    .add('#dot-p', { scale: [0, 1], duration: 500, ease: 'outBack(3)' }, '-=250')
    .call(() => loop.play());
  const loop = createTimeline({ autoplay: false, loop: true, loopDelay: 900 })
    .add('#secant-group', { opacity: [0, 1], duration: 400, ease: 'outQuad' })
    .add(state, { h: [H0, 1e-4], duration: 3600, ease: 'inOutCubic', onUpdate: render })
    .call(() => fig.classList.add('locked'))
    // (the ring's first state is invisible: a looping timeline shows each target's first state
    // again from its start, so a ring that started visible stood around P until its turn)
    .add('#pulse', { opacity: [0, 0.9], duration: 1 })
    .add('#pulse', { r: [7, 36], opacity: [0.9, 0], duration: 1100, ease: 'outQuad' })
    .add('#secant-group', { opacity: 0, duration: 450, ease: 'inQuad' }, '+=1700')
    .call(() => { fig.classList.remove('locked'); state.h = H0; render(); });

  // the rest as it scrolls into view
  const io = new IntersectionObserver(entries => {
    for (const en of entries) {
      if (!en.isIntersecting) continue;
      io.unobserve(en.target);
      if (en.target.matches('.tags, .timeline')) revealGroup(en.target);
      else rise(en.target);
    }
  }, { rootMargin: '0px 0px -12% 0px' });
  $$('main [data-reveal]').forEach(e => io.observe(e));
}

// ------------------------------------------------------------------ the tour
// the chapter in the middle of the window is the one on the calculator's screen: the new
// screenshot fades in over the old one (two images, swapped)
const chapters = $$('.chapter'), rail = $$('.rail li');
let cur = -1, front = $('#shot-a'), back = $('#shot-b');
function show(i) {
  if (i === cur) return;
  cur = i;
  chapters.forEach((c, k) => c.classList.toggle('on', k === i));
  rail.forEach((r, k) => r.classList.toggle('on', k === i));
  const src = 'media/screenshots/' + chapters[i].dataset.shot;
  if (front.getAttribute('src') === src) return;
  const incoming = back, outgoing = front;
  [front, back] = [back, front];
  incoming.src = src;
  incoming.style.zIndex = 2; outgoing.style.zIndex = 1;
  const go = () => {
    if (still) { incoming.style.opacity = 1; outgoing.style.opacity = 0; return; }
    animate(incoming, { opacity: [0, 1], scale: [1.035, 1], duration: 560, ease: 'outCubic' });
    animate(outgoing, { opacity: 0, duration: 420, ease: 'outQuad' });
  };
  (incoming.decode ? incoming.decode().catch(() => {}) : Promise.resolve()).then(go);
}
const tio = new IntersectionObserver(entries => {
  for (const en of entries) if (en.isIntersecting) show(chapters.indexOf(en.target));
}, { rootMargin: '-45% 0px -45% 0px' });
chapters.forEach(c => tio.observe(c));
show(0);

// ------------------------------------------------------------------ the versions
const more = $('#more-versions'), tl = $('.timeline');
more.addEventListener('click', () => {
  const all = !tl.classList.contains('all');
  tl.classList.toggle('all', all);
  more.setAttribute('aria-expanded', String(all));
  more.textContent = all ? 'Fewer versions' : 'All versions';
  if (all && !still) animate($$('.timeline .older'), { opacity: [0, 1], translateX: [-14, 0], duration: 600, delay: stagger(40), ease: 'outExpo' });
});
