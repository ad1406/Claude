/* Chapters 1-2 as a score. Every derivation is written as moves; each move
   is one motif. Cues are the question you would ask yourself; "why" is the
   one-line justification. Statements follow the book; where the book has a
   misprint, the piece states the corrected form and says so in its note. */
#include "content.h"
#include "stage.h"
#include <string.h>

#define NS(a) a, (int)(sizeof a / sizeof a[0])
#define KEEP -1, {0, 0, 0, 0}

const MotifInfo MOTIFS[MO_COUNT] = {
    { "Ground", "use something already given: a definition, the physics, or a fact from calculus",
      "What do we already know that applies here?",
      "Every derivation stands on a few given facts: the wave equation from physics, the nailed ends, Clairaut, the mean value theorem, the FTC, the sum-angle formulas and the definitions of $\\C$. They are the drone under the music: the fundamental of the string, sounding beneath every chord.",
      1, SC_GROUND, {0, 0, 0, 0} },
    { "Split", "rewrite a pair as its middle plus-or-minus its half-gap",
      "Are there two things whose average and half-difference would be simpler?",
      "Two numbers $a,b$ carry the same information as their middle $\\tfrac{a+b}{2}$ and half-gap $\\tfrac{a-b}{2}$, since $a=m+h$ and $b=m-h$. Sums of turns, differences of squares and pairs of waves all become products in these coordinates. It is d'Alembert's $u=x+ct$, $v=x-ct$; it is $\\gamma,\\delta$ in the beats formula; it is $\\Re z=\\tfrac{z+\\overline z}{2}$.",
      2, SC_SPLIT, {0, 0, 0, 0} },
    { "Turn", "rotate (and stretch): multiply by $re^{i\\theta}$, and add angles when turns compose",
      "Is there an angle that could be turned away, or two turns to combine?",
      "Multiplying by $e^{i\\theta}$ turns the plane by $\\theta$; multiplying by $re^{i\\theta}$ also stretches it by $r$. Turns compose by adding angles. That one fact is the sum-angle formulas, the functional equation $e^{z+w}=e^ze^w$, $i^2=-1$, and the geometry of every complex product. Even d'Alembert's coordinates are a turn by $\\pi/4$.",
      3, SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
    { "Mirror", "reflect: conjugate, flip $x\\to-x$, or pair something with its mirror image",
      "Is there a reflection hiding here, and what does a thing plus (or times) its mirror image give?",
      "A reflection pairs each thing with its image, and the pair is simpler than either half: $z+\\overline z$ is real, $z\\overline z=|z|^2$ is real and positive, and a wave meeting its inverted mirror image cancels at the mirror. Two mirrors in a row make a shift, which is why a string nailed at both ends is periodic.",
      4, SC_STRING, {3, 0, 0, 0} },
    { "Loop", "go once around: use periodicity, whole turns or winding numbers",
      "Does something come back to where it started?",
      "A periodic function lives on a circle. Going around a whole number of times brings you home, so whole turns integrate to zero, only the frequencies that fit survive (the harmonics), and periods can be added and subtracted. The minimal period is the pitch.",
      5, SC_EXP, {0, 0, 0, 0} },
    { "Hold", "freeze one variable and let the other vary",
      "If you hold one variable fixed, what is left that can still change?",
      "If something does not change as one variable moves, it can only depend on the others. That is how $\\partial_u(\\partial_v y)=0$ becomes \"a function of $v$ alone\", how $f''/f=g''/c^2g$ forces both sides to be one constant, and how a finger on the string pins every surviving mode to have a node there.",
      6, SC_HOLD, {0, 0, 0, 0} },
    { "Lanes", "work in the real and imaginary parts separately, then recombine",
      "Can this be done separately in the real and the imaginary lane?",
      "$\\C$ is $\\R^2$ with a product. Anything defined lane by lane (limits, derivatives, integrals) carries over from real analysis for free. Complexification runs the other way: lift a real problem into $\\C$, solve it with turns, then read off one lane with $\\Re$ or $\\Im$.",
      7, SC_LANES, {0, 0, 0, 0} },
    { "Shadow", "project onto the real axis, or bound something by its length",
      "Can a length be compared to its shadow?",
      "A shadow is never longer than the object: $\\Re w\\le|w|$, with equality only when $w$ points along the axis. Turn first, so that what you care about lies on the axis, then compare shadows. Every inequality in Chapter 2 (triangle inequality, integral estimate, M-L, lanes of convergent sequences) is this picture.",
      8, SC_PLANE, {PL_SHADOW, 0, 0, 0} },
};

const Section SECTIONS[] = {
    { "1.1", "Sound and the wave equation", 1 },
    { "1.2", "d'Alembert's solution", 1 },
    { "1.3", "Problems", 1 },
    { "2.1", "Combining tones", 2 },
    { "2.2", "The complex field", 2 },
    { "2.3", "Algebra and geometry", 2 },
    { "2.4", "Analysis with complex numbers", 2 },
    { "2.5", "Problems", 2 },
};
const int NSECTIONS = (int)(sizeof SECTIONS / sizeof SECTIONS[0]);

/* =================================================================== I */

static const Step S_1_24[] = {
    { MO_GROUND, "Write the wave operator from (1.1) in a shape you already know how to factor.",
      "\\Box=\\partial_x^2-\\tfrac{1}{c^2}\\partial_t^2=A^2-B^2,\\qquad A=\\partial_x,\\ \\ B=\\tfrac1c\\partial_t",
      "Move everything in (1.1) to one side: the operator is a difference of two squares.", "1.1", SC_SPACETIME, {0, 0, 0, 0} },
    { MO_SPLIT, "A difference of squares. What do the sum and the difference do?",
      "(A+B)(A-B)=A^2-AB+BA-B^2",
      "Expand sum times difference. The middle terms cancel exactly when $AB=BA$.", NULL, SC_SPACETIME, {1, 0, 0, 0} },
    { MO_GROUND, "Do the middle terms cancel when $A$ and $B$ are derivatives?",
      "AB\\,y=\\tfrac1c\\,\\partial_x\\partial_t y=\\tfrac1c\\,\\partial_t\\partial_x y=BA\\,y",
      "Clairaut-Schwarz: for $y\\in C^2$ the mixed partials agree. Commutativity is the only property of numbers the factorisation needs.", "1.3", KEEP },
    { MO_SPLIT, "So the operator factors. What does each factor do in the $(x,ct)$-plane?",
      "\\Box=\\col{1}{\\left(\\partial_x+\\tfrac1c\\partial_t\\right)}\\col{1}{\\left(\\partial_x-\\tfrac1c\\partial_t\\right)}",
      "$\\partial_x\\pm\\tfrac1c\\partial_t$ differentiates along a diagonal of the $(x,ct)$-plane: the sum and the difference of the two axis directions.", NULL, SC_SPACETIME, {1, 0, 0, 0} },
};
static const Step S_1_6[] = {
    { MO_SPLIT, "The two factors differentiate along the diagonals. Which coordinates have those diagonals as their axes?",
      "u=x+ct,\\qquad v=x-ct",
      "Sum and difference of $x$ and $ct$. Along a line $v=$ const only $u$ changes, and along $u=$ const only $v$ does.", "1.24", SC_SPACETIME, {0, 0, 0, 0} },
    { MO_SPLIT, "Can you go back?",
      "x=\\tfrac{u+v}{2},\\qquad ct=\\tfrac{u-v}{2}",
      "Middle and half-gap undo sum and difference. This pair of formulas is the Split motif in its purest form; it returns as $\\gamma,\\delta$ in the beats formula (2.1).", NULL, KEEP },
    { MO_TURN, "What does the change of coordinates look like, read in Chapter 2's language?",
      "v+iu=(1+i)\\,(x+ict)",
      "Check: $(1+i)(x+ict)=(x-ct)+i(x+ct)$. The coordinate change is multiplication by $1+i=\\sqrt2\\,e^{i\\pi/4}$: a turn by $\\pi/4$ and a stretch by $\\sqrt2$, as in Figure 1.3.", "P2.7", SC_SPACETIME, {4, 0, 0, 0} },
};
static const Step S_1_26[] = {
    { MO_GROUND, "How does a small step in $x$ move $u$ and $v$?",
      "\\frac{\\partial}{\\partial x}=\\frac{\\partial u}{\\partial x}\\frac{\\partial}{\\partial u}+\\frac{\\partial v}{\\partial x}\\frac{\\partial}{\\partial v}=\\partial_u+\\partial_v",
      "Multivariable chain rule: when $x$ increases, both $u$ and $v$ increase at rate $1$.", "1.6", SC_SPACETIME, {1, 0, 0, 0} },
    { MO_GROUND, "And a small step in $t$?",
      "\\frac{\\partial}{\\partial t}=c\\,\\partial_u-c\\,\\partial_v",
      "$u=x+ct$ grows at rate $c$; $v=x-ct$ shrinks at rate $c$.", NULL, KEEP },
    { MO_SPLIT, "Now form the two factors of $\\Box$ from (1.24).",
      "\\partial_x+\\tfrac1c\\partial_t=\\col{1}{2\\,\\partial_u},\\qquad \\partial_x-\\tfrac1c\\partial_t=\\col{1}{2\\,\\partial_v}",
      "Sum and difference isolate one new direction each: the $\\partial_v$ terms cancel in the sum, the $\\partial_u$ terms in the difference.", "1.24", KEEP },
};
static const Step S_L1_1[] = {
    { MO_GROUND, "Start from the factorisation.",
      "\\Box y=\\left(\\partial_x+\\tfrac1c\\partial_t\\right)\\left(\\partial_x-\\tfrac1c\\partial_t\\right)y",
      "Valid because $y\\in C^2$, so the derivatives commute.", "1.24", SC_SPACETIME, {1, 0, 0, 0} },
    { MO_SPLIT, "Substitute the chain rule into each factor.",
      "\\Box y=(2\\,\\partial_u)(2\\,\\partial_v)\\,y=4\\,\\frac{\\partial^2 y}{\\partial u\\,\\partial v}",
      "Each factor is a pure derivative along one characteristic direction.", "1.26", KEEP },
    { MO_GROUND, "Why is this an \"if and only if\"?",
      "\\Box y=0\\iff 4\\,\\partial_u\\partial_v y=0\\iff \\frac{\\partial^2 y}{\\partial u\\,\\partial v}=0",
      "The two sides differ by the factor $4$, and $(x,t)\\mapsto(u,v)$ is invertible.", NULL, KEEP },
};
static const Step S_1_14[] = {
    { MO_GROUND, "What does Lemma 1.1 say, written as one derivative of another?",
      "\\frac{\\partial}{\\partial u}\\left(\\frac{\\partial y}{\\partial v}\\right)=0",
      "Clairaut lets us choose the order.", "L1.1", SC_SPACETIME, {2, 0, 0, 0} },
    { MO_HOLD, "A $u$-derivative is zero everywhere. Hold $v$ fixed and walk along $u$: what can $\\partial y/\\partial v$ do?",
      "\\frac{\\partial y}{\\partial v}=\\col{5}{h(v)}",
      "Mean value theorem on each line $v=$ const: zero derivative means constant along the line. The constant may differ from line to line, so it is a function of $v$ alone.", NULL, SC_SPACETIME, {2, 0, 0, 0} },
    { MO_HOLD, "Undo the $v$-derivative.",
      "y=\\col{5}{g(v)}+\\col{5}{f(u)},\\qquad g'=h",
      "The FTC gives an antiderivative $g$ of $h$. Then $\\partial_v\\bigl(y-g(v)\\bigr)=0$, so holding $u$ fixed, $y-g(v)$ is a function of $u$ alone. Call it $f(u)$.", NULL, SC_SPACETIME, {3, 0, 0, 0} },
    { MO_TURN, "Turn back to $(x,t)$.",
      "y(x,t)=f(x+ct)+g(x-ct)",
      "$f$ keeps its shape and slides left at speed $c$; $g$ slides right. Every solution is two travelling waves passing through each other.", NULL, SC_STRING, {0, 0, 0, 0} },
};
static const Step S_1_12[] = {
    { MO_GROUND, "Impose the nail at $x=0$.",
      "0=y(0,t)=f(ct)+g(-ct)\\quad\\text{for all }t", NULL, "1.2", SC_STRING, {1, 0, 0, 0} },
    { MO_MIRROR, "Put $\\mu=-ct$. What is $g$ in terms of $f$?",
      "g(\\mu)=\\col{3}{-f(-\\mu)}",
      "$g$ is $f$ reflected left-right and flipped up-down: an odd mirror image of $f$ through the origin.", NULL, KEEP },
    { MO_MIRROR, "Substitute back into the general solution.",
      "y=f(x+ct)\\,\\col{3}{-\\,f(-x+ct)}",
      "The nail acts as a mirror. A wave arriving from the right meets its own inverted image coming out of the mirror world, and the two cancel exactly at the wall.", "1.14", KEEP },
};
static const Step S_1_13[] = {
    { MO_GROUND, "Now impose the nail at $x=\\ell$.",
      "0=y(\\ell,t)=f(\\ell+ct)-f(-\\ell+ct)", NULL, "1.2", SC_STRING, {2, 0, 0, 0} },
    { MO_MIRROR, "Read it as a statement about $f$ alone.",
      "f(\\ell+\\lambda)=f(-\\ell+\\lambda)\\quad\\text{for all }\\lambda",
      "A second mirror, at $\\ell$.", NULL, SC_STRING, {3, 0, 0, 0} },
    { MO_LOOP, "Shift the variable so that one side is just $f(\\lambda)$.",
      "f(\\lambda+2\\ell)=f(\\lambda)",
      "Replace $\\lambda$ by $\\lambda+\\ell$. Reflecting in $0$ and then in $\\ell$ moves every point by $2\\ell$. Two mirrors make a loop.", NULL, SC_STRING, {3, 0, 0, 0} },
};
static const Step S_T1_4[] = {
    { MO_GROUND, "Every $C^2$ solution of the wave equation:",
      "y=f(x+ct)+g(x-ct)", NULL, "1.14", SC_STRING, {0, 0, 0, 0} },
    { MO_MIRROR, "The nail at $0$ is a mirror:",
      "g(\\mu)=-f(-\\mu)", NULL, "1.12", SC_STRING, {1, 0, 0, 0} },
    { MO_LOOP, "The nail at $\\ell$ is a second mirror, and two mirrors are a loop:",
      "f(\\lambda+2\\ell)=f(\\lambda)", NULL, "1.13", SC_STRING, {2, 0, 0, 0} },
};
static const Step S_P1_2_1[] = {
    { MO_LOOP, "What structure do the periods have?",
      "T_1,T_2\\in\\Pi_f\\cup\\{0\\}\\implies T_1\\pm T_2\\in\\Pi_f\\cup\\{0\\}", NULL, "P1.1a", SC_PERIODS, {1, 0, 0, 0} },
    { MO_LOOP, "If periods cannot crowd near $0$, can they crowd anywhere?",
      "0\\ \\text{not a limit point of}\\ \\Pi_f\\implies T_f\\ \\text{exists}", NULL, "P1.1b", SC_PERIODS, {0, 0, 0, 0} },
    { MO_SHADOW, "Why can't periods crowd near $0$ if $f$ is continuous at $t_0$ and not constant?",
      "T_k\\to0\\implies f(t)=f(t_0)\\ \\ \\text{for all }t", NULL, "P1.1c", SC_PERIODS, {2, 0, 0, 0} },
    { MO_GROUND, "And for Riemann integrable $f$?",
      "f\\in\\mathcal{R}\\implies f\\ \\text{is continuous at some point}",
      "Lebesgue's criterion: a Riemann integrable function is continuous almost everywhere.", NULL, KEEP },
};
static const Step S_1_17[] = {
    { MO_LOOP, "$\\sin$ repeats after $2\\pi$. How fast must it wind to repeat after $2\\ell$?",
      "\\sin\\bigl(k(\\lambda+2\\ell)+\\phi\\bigr)=\\sin(k\\lambda+\\phi)\\iff 2\\ell k\\in 2\\pi\\Z",
      "The argument must advance by a whole number of turns over one period.", "1.13", SC_STANDING, {1, 0, 1, 0} },
    { MO_LOOP, "Solve for $k$.",
      "k=\\frac{n\\pi}{\\ell},\\qquad f(\\lambda)=K\\sin\\left(\\frac{n\\pi\\lambda}{\\ell}+\\phi\\right)",
      "$n$ counts the turns the sine makes in $2\\ell$. Negative $n$ only flips the sign of $K$; $n=0$ is constant.", NULL, SC_STANDING, {3, 0, 1, 0} },
};
static const Step S_1_18[] = {   /* the complex route; the trigonometric route is Problem 1.2 */
    { MO_GROUND, "Insert the sine (1.17) into Theorem 1.4 and name the angles.",
      "y=K\\left[\\sin(\\gamma+\\delta)-\\sin(\\gamma-\\delta)\\right],\\qquad \\gamma=\\tfrac{n\\pi ct}{\\ell}+\\phi,\\ \\ \\delta=\\tfrac{n\\pi x}{\\ell}",
      "The middle angle $\\gamma$ carries the time, the half-gap $\\delta$ carries the space.", "T1.4", SC_STANDING, {2, 0, 1, 0} },
    { MO_LANES, "Each sine is the imaginary lane of a turn.",
      "y=K\\,\\Im\\left\\{e^{i(\\gamma+\\delta)}-e^{i(\\gamma-\\delta)}\\right\\}", NULL, "2.36", KEEP },
    { MO_TURN, "Both turns contain $e^{i\\gamma}$. Factor it out.",
      "y=K\\,\\Im\\left\\{\\col{2}{e^{i\\gamma}}\\left(e^{i\\delta}-e^{-i\\delta}\\right)\\right\\}", NULL, "2.38", KEEP },
    { MO_MIRROR, "A unit vector minus its mirror image is...?",
      "e^{i\\delta}-e^{-i\\delta}=\\col{3}{2i\\sin\\delta}",
      "$w-\\overline w=2i\\,\\Im w$: the real parts cancel.", "2.25", KEEP },
    { MO_LANES, "Take the imaginary lane.",
      "y=2K\\sin\\delta\\;\\Im\\{ie^{i\\gamma}\\}=2K\\sin\\left(\\tfrac{n\\pi x}{\\ell}\\right)\\cos\\left(\\tfrac{n\\pi ct}{\\ell}+\\phi\\right)",
      "$\\Im\\{ie^{i\\gamma}\\}=\\Re\\,e^{i\\gamma}=\\cos\\gamma$. A shape in space times a rhythm in time: a standing wave.", NULL, SC_STANDING, {2, 0, 0, 0} },
};
static const Step S_1_19[] = {
    { MO_LOOP, "When does the time factor $\\cos\\bigl(\\tfrac{n\\pi c}{\\ell}t+\\phi\\bigr)$ come back to itself?",
      "\\frac{n\\pi c}{\\ell}\\,T_n=2\\pi\\implies T_n=\\frac{2\\ell}{nc}", NULL, "1.18", SC_STANDING, {1, 0, 0, 0} },
    { MO_LOOP, "Frequency is turns per second.",
      "\\nu_n=\\frac1{T_n}=\\frac{nc}{2\\ell}=n\\,\\nu_1,\\qquad \\nu_1=\\frac{c}{2\\ell}",
      "Equally spaced: the harmonic series. A shorter string or a larger $c$ (more tension) gives a higher pitch.", NULL, SC_STANDING, {3, 0, 0, 0} },
    { MO_LOOP, "And the space factor $\\sin(n\\pi x/\\ell)$: after what length does it repeat?",
      "\\lambda_n=\\frac{2\\ell}{n},\\qquad \\nu_n\\lambda_n=c",
      "Wavelengths $2\\ell,\\ \\ell,\\ \\tfrac{2\\ell}{3},\\dots$ are proportional to $1,\\tfrac12,\\tfrac13,\\dots$, the terms of the harmonic series $\\sum\\tfrac1n$.", NULL, SC_STANDING, {4, 0, 0, 0} },
};

/* ------------------------------------------------------------ problems I */
static const Step S_P1_1a[] = {
    { MO_LOOP, "Shift by $T_1+T_2$ in two moves.",
      "f(t+T_1+T_2)=f\\bigl((t+T_1)+T_2\\bigr)=f(t+T_1)=f(t)",
      "Use $T_2$-periodicity at the point $t+T_1$, then $T_1$-periodicity at $t$.", NULL, SC_PERIODS, {1, 0, 0, 0} },
    { MO_LOOP, "Shift backwards: undo a loop.",
      "f(t-T_1)=f\\bigl((t-T_1)+T_1\\bigr)=f(t)",
      "Apply $T_1$-periodicity at the point $t-T_1$ and read the equation from right to left.", NULL, KEEP },
    { MO_GROUND, "Name the structure.",
      "\\Pi_f\\cup\\{0\\}\\ \\text{is an additive subgroup of}\\ \\R",
      "It contains $0$ and is closed under $+$ and $-$. A subgroup of $\\R$ is either a lattice $T\\Z$ or crowds near $0$; parts (b) and (c) decide which.", NULL, KEEP },
};
static const Step S_P1_1b[] = {
    { MO_GROUND, "Suppose some $T\\neq0$ were a limit point of $\\Pi_f$.",
      "T_k\\in\\Pi_f,\\quad T_k\\ne T,\\quad T_k\\to T",
      "Argue by contradiction. Pass to a subsequence whose terms are all different.", NULL, SC_PERIODS, {0, 0, 0, 0} },
    { MO_LOOP, "Differences of periods are periods.",
      "d_k:=T_{k+1}-T_k\\in\\Pi_f,\\qquad d_k\\ne0,\\qquad d_k\\to T-T=0", "Part (a).", "P1.1a", KEEP },
    { MO_GROUND, "So what is $0$?",
      "0\\ \\text{would be a limit point of}\\ \\Pi_f",
      "That contradicts the hypothesis. So $\\Pi_f$ has no limit points at all.", NULL, KEEP },
    { MO_SHADOW, "Is the infimum of the positive periods attained?",
      "\\tau:=\\inf\\bigl(\\Pi_f\\cap(0,\\infty)\\bigr)\\in\\Pi_f",
      "The set is non-empty (if $T$ is a period, so is $-T$). If $\\tau=0$, positive periods would crowd at $0$; if $\\tau>0$ were not attained, periods would crowd at $\\tau$. Both are excluded, so $\\tau$ is the minimal period.", NULL, KEEP },
};
static const Step S_P1_1c[] = {
    { MO_GROUND, "Suppose positive periods shrink to $0$.",
      "T_k\\in\\Pi_f,\\quad T_k>0,\\quad T_k\\to0", "Contradiction again. By part (a) they may be taken positive.", NULL, SC_PERIODS, {2, 0, 0, 0} },
    { MO_LOOP, "Any $t$ can be reached from near $t_0$ by whole loops of length $T_k$.",
      "t-t_0=m_kT_k+r_k,\\qquad m_k\\in\\Z,\\ \\ 0\\le r_k<T_k",
      "Divide $t-t_0$ by $T_k$ with remainder.", NULL, KEEP },
    { MO_LOOP, "Use periodicity to slide $t$ back next to $t_0$.",
      "f(t)=f(t-m_kT_k)=f(t_0+r_k)", "$m_kT_k$ is a period by part (a).", "P1.1a", KEEP },
    { MO_SHADOW, "Let $k\\to\\infty$.",
      "|f(t)-f(t_0)|=|f(t_0+r_k)-f(t_0)|<\\varepsilon\\quad(k\\ \\text{large})",
      "$r_k\\to0$ and $f$ is continuous at $t_0$. As $\\varepsilon$ is arbitrary, $f(t)=f(t_0)$ for every $t$: $f$ is constant, a contradiction.", NULL, KEEP },
    { MO_GROUND, "Finish Proposition 1.2.1.",
      "\\text{(c)}\\ \\&\\ \\text{(b)}\\implies T_f\\ \\text{exists}", NULL, "P1.1b", KEEP },
};
static const Step S_P1_2[] = {   /* the trigonometric route */
    { MO_GROUND, "Insert $f(\\lambda)=K\\sin(\\tfrac{n\\pi\\lambda}{\\ell}+\\phi)$ into Theorem 1.4.",
      "y=K\\sin\\left(\\tfrac{n\\pi(ct+x)}{\\ell}+\\phi\\right)-K\\sin\\left(\\tfrac{n\\pi(ct-x)}{\\ell}+\\phi\\right)", NULL, "T1.4", SC_STANDING, {2, 0, 1, 0} },
    { MO_SPLIT, "Two angles. What are their middle and half-gap?",
      "\\col{1}{\\gamma}=\\tfrac{n\\pi ct}{\\ell}+\\phi,\\quad \\col{1}{\\delta}=\\tfrac{n\\pi x}{\\ell}:\\qquad y=K\\left[\\sin(\\gamma+\\delta)-\\sin(\\gamma-\\delta)\\right]",
      "Time sits in the middle angle, space in the half-gap.", NULL, KEEP },
    { MO_TURN, "Expand both with the sum-angle formula.",
      "\\sin(\\gamma\\pm\\delta)=\\sin\\gamma\\cos\\delta\\pm\\sin\\delta\\cos\\gamma",
      "(2.3) with $b=\\pm\\delta$, using that $\\cos$ is even and $\\sin$ odd.", "2.3", KEEP },
    { MO_MIRROR, "Subtract. What survives the difference of $\\delta$ and its mirror $-\\delta$?",
      "y=2K\\sin\\delta\\cos\\gamma=2K\\sin\\left(\\tfrac{n\\pi x}{\\ell}\\right)\\cos\\left(\\tfrac{n\\pi ct}{\\ell}+\\phi\\right)",
      "The parts even in $\\delta$ cancel and the odd parts double. That is (1.18).", "1.18", SC_STANDING, {2, 0, 0, 0} },
};
static const Step S_P1_3a[] = {
    { MO_GROUND, "Insert the product $y=f(x)g(t)$ into (1.1).",
      "f''(x)\\,g(t)=\\frac1{c^2}\\,f(x)\\,g''(t)", NULL, "1.1", SC_SEPARATION, {4, 0, 0, 0} },
    { MO_GROUND, "Separate: put everything with $x$ on one side.",
      "\\frac{f''(x)}{f(x)}=\\frac1{c^2}\\frac{g''(t)}{g(t)}", "Divide by $f(x)g(t)$ where it is not zero.", NULL, KEEP },
    { MO_HOLD, "Hold $t$ fixed and move $x$. What can the left side do?",
      "\\frac{f''(x)}{f(x)}=\\frac1{c^2}\\frac{g''(t)}{g(t)}=\\col{5}{-k}",
      "The right side does not see $x$, so the left side cannot change with $x$; symmetrically in $t$. A function of $x$ that equals a function of $t$ is a constant. The minus sign is just a convenient name.", NULL, KEEP },
    { MO_GROUND, "Write out the two equations.",
      "g''=-kc^2\\,g,\\qquad f''=-k\\,f", "One PDE has become two ODEs.", NULL, KEEP },
};
static const Step S_P1_3b[] = {
    { MO_GROUND, "Impose the nail at $0$ on the product.",
      "0=y(0,t)=f(0)\\,g(t)\\quad\\text{for all }t", NULL, "1.2", SC_SEPARATION, {4, 0, 0, 0} },
    { MO_HOLD, "Hold $x=0$; $t$ is still free. Which factor has to vanish?",
      "g\\not\\equiv0\\implies f(0)=0",
      "Choose $t$ with $g(t)\\ne0$; one exists, since otherwise $y\\equiv0$.", NULL, KEEP },
    { MO_HOLD, "Same at $x=\\ell$.",
      "f(0)=f(\\ell)=0", NULL, NULL, KEEP },
};
static const Step S_P1_3c[] = {
    { MO_GROUND, "Case $k<0$: write $k=-\\mu^2$ with $\\mu>0$.",
      "f=Ae^{\\mu x}+Be^{-\\mu x},\\quad f(0)=f(\\ell)=0\\implies A=B=0",
      "$A+B=0$ and $A(e^{\\mu\\ell}-e^{-\\mu\\ell})=0$ with $\\mu\\ell\\ne0$.", NULL, SC_SEPARATION, {-1, 0, 0, 0} },
    { MO_GROUND, "Case $k=0$.",
      "f=A+Bx,\\quad f(0)=f(\\ell)=0\\implies f\\equiv0", NULL, NULL, SC_SEPARATION, {0, 0, 0, 0} },
    { MO_TURN, "Case $k>0$: the solutions turn.",
      "f=A\\cos(\\sqrt k\\,x)+B\\sin(\\sqrt k\\,x),\\quad f(0)=0\\implies A=0", NULL, NULL, SC_SEPARATION, {2.3f, 0, 0, 0} },
    { MO_LOOP, "The sine must return to zero exactly at $\\ell$.",
      "\\sin(\\sqrt k\\,\\ell)=0\\iff\\sqrt k\\,\\ell=n\\pi\\iff k=k_n=\\left(\\tfrac{n\\pi}{\\ell}\\right)^2",
      "A whole number of half-turns fits on the string; $n\\ge1$.", NULL, SC_SEPARATION, {4, 0, 0, 0} },
    { MO_LOOP, "Which frequencies does that allow?",
      "\\nu_n=\\frac{c}{2\\pi}\\sqrt{k_n}=\\frac{nc}{2\\ell}",
      "The time equation $g''=-k_nc^2g$ oscillates at angular frequency $c\\sqrt{k_n}$: the harmonics (1.19).", "1.19", KEEP },
};
static const Step S_P1_3d[] = {
    { MO_TURN, "Solve the time equation $g''=-k_nc^2g$.",
      "g=A\\cos\\omega t+B\\sin\\omega t,\\qquad \\omega=\\frac{n\\pi c}{\\ell}", NULL, NULL, SC_PLANE, {PL_POLAR, 0, 0, 0} },
    { MO_TURN, "Combine the two into one shifted sine. Think of $(B,A)$ as a point in the plane.",
      "A\\cos\\omega t+B\\sin\\omega t=C\\sin(\\omega t+\\phi),\\qquad B+iA=Ce^{i\\phi}",
      "Polar form of the point $B+iA$: $C=\\sqrt{A^2+B^2}$, $\\phi=\\arg(B+iA)$. Expand $C\\sin(\\omega t+\\phi)$ to check.", "2.35", KEEP },
    { MO_GROUND, "Multiply by $f$.",
      "y=f(x)g(t)=C\\sin\\left(\\tfrac{n\\pi x}{\\ell}\\right)\\sin\\left(\\tfrac{n\\pi ct}{\\ell}+\\phi\\right)",
      "With $\\sin(\\theta+\\tfrac\\pi2)=\\cos\\theta$ these are exactly the Bernoulli solutions (1.18).", "1.18", SC_STANDING, {2, 0, 0, 0} },
};
static const Step S_P1_4[] = {
    { MO_LOOP, "Where are the nodes of $f_n(x)=\\sin(n\\pi x/\\ell)$?",
      "f_n(x)=0\\iff x=\\frac{j\\ell}{n},\\quad j=0,1,\\dots,n",
      "$n$ half-turns along the string: $n+1$ equally spaced nodes.", NULL, SC_STANDING, {4, 0, 0, 0} },
    { MO_HOLD, "A light finger at $x=\\ell/N$ holds that point still for all $t$. Which modes allow that?",
      "\\sin\\left(\\frac{n\\pi}{N}\\right)=0\\iff N\\mid n",
      "A mode survives only if it already has a node under the finger.", NULL, SC_STANDING, {4, 3, 0, 0} },
    { MO_LOOP, "What do you hear?",
      "\\nu_N,\\ \\nu_{2N},\\ \\nu_{3N},\\ \\dots\\ =\\ N\\nu_1,\\ 2N\\nu_1,\\ 3N\\nu_1,\\ \\dots",
      "The lowest survivor $\\nu_N=N\\nu_1$ sets the pitch: an overtone.", "1.19", SC_STANDING, {3, 3, 0, 0} },
};

/* ================================================================== II */

static const Step S_2_1[] = {
    { MO_LANES, "Each sine is the imaginary lane of a unit vector.",
      "\\sin\\alpha+\\sin\\beta=\\Im\\left\\{e^{i\\alpha}+e^{i\\beta}\\right\\}",
      "Complexify: solve the problem in $\\C$ and come back with $\\Im$ (2.40).", "2.36", SC_PHASORS, {1, 0, 0, 0} },
    { MO_SPLIT, "Two angles. Name their middle and half-gap.",
      "\\alpha=\\col{1}{\\gamma}+\\col{1}{\\delta},\\ \\ \\beta=\\col{1}{\\gamma}-\\col{1}{\\delta},\\qquad \\gamma=\\tfrac{\\alpha+\\beta}{2},\\ \\ \\delta=\\tfrac{\\alpha-\\beta}{2}",
      "In Figure 2.2, $\\gamma$ is the direction of the parallelogram's diagonal.", "1.6", KEEP },
    { MO_TURN, "Both vectors contain the turn $e^{i\\gamma}$. Factor it out.",
      "e^{i\\alpha}+e^{i\\beta}=\\col{2}{e^{i\\gamma}}\\left(e^{i\\delta}+e^{-i\\delta}\\right)", NULL, "2.38", KEEP },
    { MO_MIRROR, "$e^{-i\\delta}$ is the mirror image of $e^{i\\delta}$. What is a vector plus its mirror image?",
      "e^{i\\delta}+e^{-i\\delta}=\\col{3}{2\\cos\\delta}", "$w+\\overline w=2\\Re w$: the imaginary parts cancel.", "2.25", KEEP },
    { MO_LANES, "Take the imaginary lane.",
      "\\sin\\alpha+\\sin\\beta=2\\cos\\delta\\;\\Im e^{i\\gamma}=2\\cos\\left(\\tfrac{\\alpha-\\beta}{2}\\right)\\sin\\left(\\tfrac{\\alpha+\\beta}{2}\\right)",
      "$2\\cos\\delta$ is real, so it passes through $\\Im$. The real lane gives (2.2) for cosines.", NULL, SC_PHASORS, {0, 2, 0, 0} },
};
static const Step S_BEATS[] = {
    { MO_SPLIT, "Apply (2.1) to two tones $\\nu_0,\\nu_1$.",
      "\\alpha=2\\pi\\nu_1t,\\ \\beta=2\\pi\\nu_0t:\\qquad \\gamma=2\\pi\\tfrac{\\nu_0+\\nu_1}{2}t,\\ \\ \\delta=2\\pi\\tfrac{\\nu_1-\\nu_0}{2}t", NULL, "2.1", SC_PHASORS, {0, 2, 0, 0} },
    { MO_SPLIT, "Which factor is fast, and which is slow?",
      "\\under{2\\cos\\left(2\\pi\\tfrac{\\nu_1-\\nu_0}{2}t\\right)}{\\text{envelope: half-gap}}\\cdot\\under{\\sin\\left(2\\pi\\tfrac{\\nu_0+\\nu_1}{2}t\\right)}{\\text{pitch: middle}}",
      "For $440$ and $442$ Hz: a $441$ Hz tone whose loudness swells and fades with a $1$ Hz cosine.", NULL, KEEP },
    { MO_LOOP, "How many beats per second? Loudness peaks when $|\\cos|$ does.",
      "\\bigl|\\cos(\\pi(\\nu_1-\\nu_0)t)\\bigr|\\ \\text{has period}\\ \\frac1{\\nu_1-\\nu_0}\\implies \\nu_1-\\nu_0\\ \\text{beats per second}",
      "$|\\cos|$ repeats twice as often as $\\cos$. That is the extra factor $2$ in Experiment 2.1.1.", NULL, SC_PHASORS, {0, 4, 0, 0} },
};
static const Step S_2_12[] = {
    { MO_TURN, "We want a product in which multiplying by $\\hat\\imath$ is a quarter turn.",
      "\\hat\\imath\\cdot\\pmat{c}{d}=\\pmat{-d}{c}",
      "A quarter turn sends $(1,0)\\mapsto(0,1)$ and $(0,1)\\mapsto(-1,0)$.", NULL, SC_PLANE, {PL_ITURN, 0, 0, 0} },
    { MO_LANES, "Multiplying by $\\hat1$ should change nothing. Distribute over $z=a\\hat1+b\\hat\\imath$.",
      "(a\\hat1+b\\hat\\imath)\\cdot\\pmat{c}{d}=a\\pmat{c}{d}+b\\pmat{-d}{c}",
      "Ask the product to distribute and to agree with scaling by reals.", NULL, KEEP },
    { MO_GROUND, "Add the lanes.",
      "\\pmat{a}{b}\\cdot\\pmat{c}{d}=\\pmat{ac-bd}{ad+bc}",
      "That is (2.12). The definition is forced by two wishes: distributivity, and \"$\\hat\\imath$ is a quarter turn\".", NULL, SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
};
static const Step S_2_16[] = {
    { MO_GROUND, "Plug the units into (2.12).",
      "\\pmat{0}{1}\\cdot\\pmat{0}{1}=\\pmat{0\\cdot0-1\\cdot1}{0\\cdot1+1\\cdot0}=\\pmat{-1}{0}", NULL, "2.12", SC_PLANE, {PL_ITURN, 0, 0, 0} },
    { MO_TURN, "See it.",
      "\\hat\\imath^2=\\text{two quarter turns}=\\text{a half turn}=-\\hat1",
      "$i^2=-1$ is no mystery: turning twice by $90^\\circ$ points you backwards. Likewise $\\hat1\\hat1=\\hat1$ and $\\hat1\\hat\\imath=\\hat\\imath$.", NULL, KEEP },
};
static const Step S_2_18[] = {
    { MO_MIRROR, "What can you multiply $z$ by to get a real number?",
      "z\\,\\overline z=(a+ib)(a-ib)=a^2+b^2", NULL, "2.31", SC_PLANE, {PL_MODSQ, 0, 0, 0} },
    { MO_GROUND, "Divide by that real number.",
      "z\\cdot\\frac{\\overline z}{a^2+b^2}=1\\implies z^{-1}=\\frac{a-ib}{a^2+b^2}",
      "Dividing by a non-zero real is all we need. Problem 2.1(b) checks this with (2.12).", NULL, SC_PLANE, {PL_INVERSE, 0, 0, 0} },
    { MO_TURN, "What does $z^{-1}$ look like?",
      "z=re^{i\\theta}\\implies z^{-1}=\\tfrac1r\\,e^{-i\\theta}",
      "Undo the turn (mirror the angle) and invert the length.", "P2.7", KEEP },
};
static const Step S_2_23[] = {
    { MO_LANES, "Distribute, treating $i$ like a letter.",
      "(a+ib)(c+id)=ac+i\\,ad+i\\,bc+i^2\\,bd", "Distributive law (2.20).", NULL, SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
    { MO_TURN, "Replace $i^2$.",
      "=(ac-bd)+i(ad+bc)", "This reproduces (2.12), so there is nothing to memorise.", "2.16", KEEP },
};
static const Step S_2_25[] = {
    { MO_MIRROR, "Add $z$ to its mirror image.",
      "z+\\overline z=(a+ib)+(a-ib)=2a", "The parts perpendicular to the mirror cancel.", "2.24", SC_PLANE, {PL_REIM, 0, 0, 0} },
    { MO_SPLIT, "Middle and half-gap of the pair $z,\\overline z$.",
      "\\frac{z+\\overline z}{2}=a=\\Re z,\\qquad \\frac{z-\\overline z}{2i}=\\frac{2ib}{2i}=b=\\Im z",
      "The middle lands on the mirror; the half-gap is perpendicular to it.", NULL, KEEP },
};
static const Step S_2_31[] = {
    { MO_MIRROR, "Multiply $z$ by its mirror image.",
      "z\\,\\overline z=(a+ib)(a-ib)", NULL, "2.24", SC_PLANE, {PL_MODSQ, 0, 0, 0} },
    { MO_SPLIT, "A sum times a difference. You have seen this shape before.",
      "=a^2-(ib)^2=a^2+b^2=|z|^2",
      "The same $(A+B)(A-B)=A^2-B^2$ that factored the wave operator in (1.24), now with $B=ib$.", "1.24", KEEP },
    { MO_TURN, "Why is it real, geometrically?",
      "re^{i\\theta}\\cdot re^{-i\\theta}=r^2e^{0}=r^2", "A turn and its mirror image cancel.", "P2.7", KEEP },
};
static const Step S_2_28[] = {
    { MO_MIRROR, "Square, and use the mirror.",
      "|zw|^2=zw\\,\\overline{zw}", NULL, "2.31", SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
    { MO_MIRROR, "Is the mirror image of a product the product of the mirror images?",
      "\\overline{zw}=\\overline z\\,\\overline w",
      "Check with (2.23): $(ac-bd)-i(ad+bc)=(a-ib)(c-id)$.", "2.23", KEEP },
    { MO_GROUND, "Rearrange (the product commutes).",
      "|zw|^2=z\\overline z\\;w\\overline w=|z|^2|w|^2", "Take square roots of non-negative reals.", NULL, KEEP },
    { MO_TURN, "See it.",
      "\\left|r_1e^{i\\theta_1}\\,r_2e^{i\\theta_2}\\right|=r_1r_2",
      "Turns don't change length; only the stretches multiply. Quotients: apply this to $\\tfrac zw\\cdot w=z$. And the mirror passes through $0$, so $|\\overline z|=|z|$.", "P2.7", KEEP },
};
static const Step S_2_32[] = {
    { MO_MIRROR, "Lengths are awkward; squared lengths are products with the mirror image.",
      "|z+w|^2=(z+w)(\\overline z+\\overline w)", NULL, "2.31", SC_PLANE, {PL_TRIANGLE, 0, 0, 0} },
    { MO_LANES, "Expand, and pair up the cross terms.",
      "=|z|^2+\\bigl(z\\overline w+\\overline{z\\overline w}\\bigr)+|w|^2=|z|^2+2\\Re(z\\overline w)+|w|^2",
      "The cross terms are a number and its mirror image: twice its real part.", "2.25", KEEP },
    { MO_SHADOW, "Compare the real part with the length.",
      "\\Re(z\\overline w)\\le|z\\overline w|=|z||w|", "A shadow is never longer than the vector.", "2.28", SC_PLANE, {PL_SHADOW, 0, 0, 0} },
    { MO_GROUND, "Complete the square.",
      "|z+w|^2\\le(|z|+|w|)^2\\implies|z+w|\\le|z|+|w|", NULL, NULL, SC_PLANE, {PL_TRIANGLE, 0, 0, 0} },
};
static const Step S_2_33[] = {
    { MO_GROUND, "Write $z$ as a detour through $w$.",
      "z=(z-w)+w", NULL, NULL, SC_PLANE, {PL_TRIANGLE, 1, 0, 0} },
    { MO_SHADOW, "Apply the triangle inequality to the detour.",
      "|z|\\le|z-w|+|w|\\implies|z|-|w|\\le|z-w|", NULL, "2.32", KEEP },
    { MO_MIRROR, "Swap the roles of $z$ and $w$.",
      "|w|-|z|\\le|w-z|=|z-w|", "Together the two inequalities bound $\\bigl||z|-|w|\\bigr|$.", NULL, KEEP },
};
static const Step S_2_34[] = {
    { MO_MIRROR, "Make the denominator real.",
      "\\frac{z}{w}=\\frac{z\\,\\overline w}{w\\,\\overline w}=\\frac{(a+ib)(c-id)}{c^2+d^2}", NULL, "2.31", SC_PLANE, {PL_DIVIDE, 0, 0, 0} },
    { MO_LANES, "Multiply out the numerator and sort into lanes.",
      "=\\frac{ac+bd}{c^2+d^2}+i\\,\\frac{bc-ad}{c^2+d^2}", NULL, "2.23", KEEP },
};
static const Step S_2_35[] = {
    { MO_GROUND, "Polar coordinates of the point $(a,b)$.",
      "a=r\\cos\\theta,\\quad b=r\\sin\\theta,\\qquad r=|z|", NULL, NULL, SC_PLANE, {PL_POLAR, 0, 0, 0} },
    { MO_TURN, "Name the unit vector at angle $\\theta$.",
      "z=r(\\cos\\theta+i\\sin\\theta)=r\\,e^{i\\theta}",
      "Length times direction. The angle is only defined modulo $2\\pi$, which is a loop.", "2.36", KEEP },
};
static const Step S_2_38[] = {
    { MO_LANES, "Split $z=a+ib$ and $w=c+id$ into lanes.",
      "e^{z+w}=e^{(a+c)+i(b+d)}=e^{a+c}\\,e^{i(b+d)}", NULL, "2.37", SC_EXP, {3, 0, 0, 0} },
    { MO_GROUND, "The real lane: you know this.",
      "e^{a+c}=e^a\\,e^c", "The real exponential law.", NULL, KEEP },
    { MO_TURN, "The turning part: multiply out $e^{ib}e^{id}$.",
      "e^{ib}e^{id}=(\\cos b\\cos d-\\sin b\\sin d)+i(\\sin b\\cos d+\\cos b\\sin d)", NULL, "2.23", KEEP },
    { MO_TURN, "Recognise the brackets.",
      "=\\cos(b+d)+i\\sin(b+d)=e^{i(b+d)}", "Sum-angle formulas: turns compose by adding angles.", "2.3", KEEP },
    { MO_GROUND, "Regroup.",
      "e^{z+w}=e^ae^{ib}\\;e^ce^{id}=e^z\\,e^w", "The product commutes.", NULL, KEEP },
};
static const Step S_2_42[] = {
    { MO_LANES, "Read the two lanes.",
      "\\Re\\,e^{it}=\\cos t,\\qquad \\Im\\,e^{it}=\\sin t", NULL, "2.36", SC_EXP, {0, 0, 0, 0} },
    { MO_LOOP, "Where does it go?",
      "|e^{it}|=1,\\qquad e^{i(t+2\\pi)}=e^{it}",
      "Once around the unit circle every $2\\pi$: the model of all periodicity, and the reason tones and turns are the same subject.", NULL, KEEP },
};
static const Step S_2_45[] = {
    { MO_LANES, "Only complex scalars are new. Write $\\alpha=p+iq$ and $f=u+iv$.",
      "\\alpha f=(pu-qv)+i(pv+qu)", NULL, "2.23", SC_LANES, {3, 0, 0, 0} },
    { MO_LANES, "Integrate lane by lane, using real linearity.",
      "\\int\\alpha f=\\Bigl(p\\!\\int\\! u-q\\!\\int\\! v\\Bigr)+i\\Bigl(p\\!\\int\\! v+q\\!\\int\\! u\\Bigr)", NULL, "2.43", KEEP },
    { MO_GROUND, "Recombine.",
      "=(p+iq)\\Bigl(\\int u+i\\int v\\Bigr)=\\alpha\\int f", "Additivity in $f$ and $g$ holds lane by lane directly.", "2.23", KEEP },
};
static const Step S_2_47[] = {
    { MO_LANES, "Write $\\lambda=a+ib$ and read off the lanes.",
      "e^{\\lambda t}=e^{at}\\cos bt+i\\,e^{at}\\sin bt", NULL, "2.37", SC_EXP, {1, 0, 0, 0} },
    { MO_LANES, "Differentiate each lane (product rule).",
      "\\frac{d}{dt}e^{\\lambda t}=\\bigl(ae^{at}\\cos bt-be^{at}\\sin bt\\bigr)+i\\bigl(ae^{at}\\sin bt+be^{at}\\cos bt\\bigr)", NULL, "2.43", KEEP },
    { MO_TURN, "Is that a product of two complex numbers? Compare with (2.23).",
      "=(a+ib)\\bigl(e^{at}\\cos bt+ie^{at}\\sin bt\\bigr)=\\lambda\\,e^{\\lambda t}",
      "The velocity is the position turned by $\\arg\\lambda$ and stretched by $|\\lambda|$. For $\\lambda=i$ the velocity is perpendicular to the position, so the point circles.", "2.23", KEEP },
};
static const Step S_2_48[] = {
    { MO_HOLD, "Guess an antiderivative from (2.47).",
      "G(t)=\\frac1\\lambda e^{\\lambda t},\\qquad G'(t)=e^{\\lambda t}", NULL, "2.47", SC_EXP, {1, 0, 0, 0} },
    { MO_GROUND, "Apply FTC-3.",
      "\\int_a^be^{\\lambda t}\\,dt=G(b)-G(a)=\\frac{1}{\\lambda}\\left(e^{\\lambda b}-e^{\\lambda a}\\right)", NULL, "FTC", KEEP },
};
static const Step S_P2_8[] = {
    { MO_LANES, "Write $\\lambda=a+ib$ and read off the lanes.",
      "e^{\\lambda t}=e^{at}\\cos bt+i\\,e^{at}\\sin bt", NULL, "2.37", SC_EXP, {1, 0, 0, 0} },
    { MO_LANES, "Differentiate each lane (product rule).",
      "\\frac{d}{dt}e^{\\lambda t}=\\bigl(ae^{at}\\cos bt-be^{at}\\sin bt\\bigr)+i\\bigl(ae^{at}\\sin bt+be^{at}\\cos bt\\bigr)", NULL, "2.43", KEEP },
    { MO_TURN, "Is that a product of two complex numbers? Compare with (2.23).",
      "=(a+ib)\\bigl(e^{at}\\cos bt+ie^{at}\\sin bt\\bigr)=\\lambda\\,e^{\\lambda t}",
      "That is (2.47). Velocity is the position turned by $\\arg\\lambda$ and stretched by $|\\lambda|$.", "2.23", KEEP },
    { MO_HOLD, "Now (2.48): guess an antiderivative.",
      "G(t)=\\frac1\\lambda e^{\\lambda t},\\qquad G'(t)=e^{\\lambda t}\\quad(\\lambda\\ne0)", NULL, NULL, KEEP },
    { MO_GROUND, "Apply FTC-3.",
      "\\int_a^be^{\\lambda t}\\,dt=\\frac{1}{\\lambda}\\left(e^{\\lambda b}-e^{\\lambda a}\\right)", NULL, "FTC", KEEP },
};
static const Step S_2_50[] = {
    { MO_MIRROR, "What is the mirror image of $e^{2\\pi inx}$?",
      "\\overline{e_n(x)}=e^{-2\\pi inx}=e_{-n}(x)", "Conjugation reverses the direction of turning.", "2.24", SC_WINDING, {0, 2, 0, 0} },
    { MO_TURN, "Combine the two turns.",
      "\\overline{e_n(x)}\\,e_m(x)=e^{2\\pi i(m-n)x}", NULL, "2.38", KEEP },
    { MO_LOOP, "For $m\\neq n$ the integrand winds $m-n$ whole times around the circle as $x$ runs over $[0,1]$.",
      "\\int_0^1e^{2\\pi i(m-n)x}\\,dx=\\left.\\frac{e^{2\\pi i(m-n)x}}{2\\pi i(m-n)}\\right|_0^1=0",
      "Whole turns end where they started, so the antiderivative takes the same value at $0$ and $1$. In the picture: points spread evenly round a circle average to its centre.", "2.48", KEEP },
    { MO_GROUND, "For $m=n$ the integrand is $e^0$.",
      "\\int_0^11\\,dx=1\\qquad\\text{so}\\qquad\\int_0^1\\overline{e_n}\\,e_m=\\delta_{n,m}", NULL, NULL, SC_WINDING, {0, 0, 0, 0} },
};
static const Step S_T2_5ii[] = {
    { MO_TURN, "The integral is some complex number. Write it in polar form.",
      "\\int_a^bf(t)\\,dt=re^{i\\theta}", NULL, "2.35", SC_POLARIZE, {0, 0, 0, 0} },
    { MO_TURN, "Turn it back onto the positive real axis.",
      "r=\\col{2}{e^{-i\\theta}}\\int_a^bf=\\int_a^b\\col{2}{e^{-i\\theta}}f(t)\\,dt", "Linearity (2.45) moves the constant inside.", "2.45", SC_POLARIZE, {1, 0, 0, 0} },
    { MO_LANES, "$r$ is real, so it equals its own real part. Take $\\Re$ lane by lane.",
      "r=\\Re\\int_a^be^{-i\\theta}f=\\int_a^b\\Re\\bigl(e^{-i\\theta}f(t)\\bigr)\\,dt", NULL, "2.43", KEEP },
    { MO_SHADOW, "Compare each shadow with its length.",
      "\\Re\\bigl(e^{-i\\theta}f(t)\\bigr)\\le\\bigl|e^{-i\\theta}f(t)\\bigr|=|f(t)|", NULL, NULL, SC_POLARIZE, {2, 0, 0, 0} },
    { MO_GROUND, "Integrate the inequality.",
      "\\left|\\int_a^bf\\right|=r\\le\\int_a^b|f(t)|\\,dt",
      "Monotonicity of real integrals; $|f|$ is integrable by part (i).", "T2.5i", KEEP },
};
static const Step S_T2_5i[] = {
    { MO_LANES, "Write the length through the lanes.",
      "|f|=\\sqrt{u^2+v^2},\\qquad u=\\Re f,\\ v=\\Im f", NULL, NULL, SC_LANES, {0, 0, 0, 0} },
    { MO_GROUND, "Compose an integrable function with a continuous one.",
      "g=u^2+v^2\\in\\mathcal R,\\ \\ \\phi=\\sqrt{\\cdot}\\ \\text{continuous}\\implies\\phi\\circ g=|f|\\in\\mathcal R", NULL, "Pr2.4.1", KEEP },
    { MO_GROUND, "For piecewise continuous $f$ there is nothing to prove.",
      "f\\in\\mathcal{PC}\\implies|f|\\in\\mathcal{PC}\\subseteq\\mathcal R", "Continuous on each piece, with one-sided limits at the joins.", "D2.6", SC_RIEMANN, {4, 0, 0, 0} },
};
static const Step S_P2_4_1[] = {
    { MO_SHADOW, "Few intervals can have large fluctuation of $g$.",
      "\\sum_{j\\in\\mathcal B_\\delta}\\Delta x_j\\le\\frac\\eta\\delta", NULL, "P2.9a", SC_RIEMANN, {1, 0, 0, 0} },
    { MO_SHADOW, "Bound $U-L$ for $\\phi\\circ g$ on good and bad intervals.",
      "U(\\phi\\circ g;P)-L(\\phi\\circ g;P)\\le\\varepsilon(b-a)+2\\|\\phi\\circ g\\|_\\infty\\frac\\eta\\delta", NULL, "P2.9b", KEEP },
};
static const Step S_C2_4_1[] = {
    { MO_SHADOW, "Triangle inequality twice: for the finite sum, then inside each integral.",
      "\\left|\\int_Sf\\right|\\le\\sum_j\\left|\\int_{t_j}^{t_{j+1}}f\\right|\\le\\sum_j\\int_{t_j}^{t_{j+1}}|f|", NULL, "T2.5ii", SC_RIEMANN, {2, 0, 0, 0} },
    { MO_SHADOW, "Bound the integrand uniformly.",
      "|f(t)|\\le\\|f\\|_{S;\\infty}=:M\\ \\text{on}\\ S\\implies\\int_{t_j}^{t_{j+1}}|f|\\le M\\,(t_{j+1}-t_j)", NULL, "2.51", KEEP },
    { MO_GROUND, "Add up the lengths.",
      "\\left|\\int_Sf\\right|\\le M\\sum_j(t_{j+1}-t_j)=M\\cdot|S|",
      "Finding $M$ is the work; the length $L=|S|$ usually comes free.", NULL, KEEP },
};

/* ----------------------------------------------------------- problems II */
static const Step S_P2_1[] = {
    { MO_GROUND, "(a) Commutativity: swap the factors in (2.12).",
      "(c,d)\\cdot(a,b)=(ca-db,\\ cb+da)=(ac-bd,\\ ad+bc)", "Real multiplication commutes in each lane.", "2.12", SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
    { MO_TURN, "(a) Associativity. See it before you expand it.",
      "(z_1z_2)z_3=r_1r_2r_3\\,e^{i(\\theta_1+\\theta_2+\\theta_3)}=z_1(z_2z_3)",
      "Composing turns and stretches is associative. Expanding with (2.12), both sides give $(ace-adf-bcf-bde,\\ acf+ade+bce-bdf)$.", "P2.7", KEEP },
    { MO_MIRROR, "(b) Plug $z^{-1}=\\tfrac{1}{a^2+b^2}(a,-b)$ into (2.12).",
      "\\frac{1}{a^2+b^2}\\pmat{a\\cdot a-(-b)\\,b}{a\\,b+(-b)\\,a}=\\pmat{1}{0}", NULL, "2.18", SC_PLANE, {PL_INVERSE, 0, 0, 0} },
    { MO_LANES, "(c) Distributivity: (2.12) is linear in each factor.",
      "(z_1+z_2)\\,w=\\pmat{(a_1+a_2)c-(b_1+b_2)d}{(a_1+a_2)d+(b_1+b_2)c}=z_1w+z_2w", NULL, NULL, SC_PLANE, {PL_SUM, 0, 0, 0} },
    { MO_TURN, "(d) The units.",
      "\\hat1\\hat1=\\hat1,\\qquad \\hat\\imath\\hat\\imath=-\\hat1,\\qquad \\hat1\\hat\\imath=\\hat\\imath\\hat1=\\hat\\imath", NULL, "2.16", SC_PLANE, {PL_ITURN, 0, 0, 0} },
};
static const Step S_P2_2[] = {
    { MO_MIRROR, "(a) Add and subtract the mirror image.",
      "z+\\overline z=2a,\\qquad z-\\overline z=2ib", NULL, "2.25", SC_PLANE, {PL_REIM, 0, 0, 0} },
    { MO_MIRROR, "(b) Multiply by the mirror image.",
      "z\\overline z=a^2+b^2=|z|^2", NULL, "2.31", SC_PLANE, {PL_MODSQ, 0, 0, 0} },
    { MO_MIRROR, "(c) Products and quotients.",
      "|zw|^2=zw\\,\\overline{zw}=z\\overline z\\,w\\overline w,\\qquad \\left|\\tfrac zw\\right||w|=|z|",
      "For quotients, apply the product rule to $\\tfrac zw\\cdot w=z$.", "2.28", SC_PLANE, {PL_DIVIDE, 0, 0, 0} },
    { MO_MIRROR, "(d) Conjugation preserves length. Compute it, then draw it.",
      "|\\overline z|^2=a^2+(-b)^2=|z|^2",
      "The mirror passes through $0$, so reflecting cannot change the distance to $0$.", NULL, SC_PLANE, {PL_CONJ, 0, 0, 0} },
};
static const Step S_P2_3[] = {
    { MO_TURN, "(a)(i) Simplify the powers of $i$: $i^2=-1$, $i^3=-i$.",
      "\\frac{i^2}{i^3-4i+6}=\\frac{-1}{6-5i}", NULL, "2.16", SC_PLANE, {PL_ITURN, 0, 0, 0} },
    { MO_MIRROR, "Make the denominator real.",
      "\\frac{-1}{6-5i}\\cdot\\frac{6+5i}{6+5i}=\\frac{-6-5i}{61}",
      "So $\\Re=-\\tfrac{6}{61}$ and $\\Im=-\\tfrac{5}{61}$.", "2.34", SC_PLANE, {PL_DIVIDE, 0, 0, 0} },
    { MO_LANES, "(a)(ii) Split the exponent into lanes.",
      "e^{4(2+\\sqrt2\\,i)t}=e^{8t}\\left(\\cos(4\\sqrt2\\,t)+i\\sin(4\\sqrt2\\,t)\\right)",
      "$\\Re=e^{8t}\\cos(4\\sqrt2\\,t)$, $\\Im=e^{8t}\\sin(4\\sqrt2\\,t)$: an outward spiral.", "2.37", SC_EXP, {1, 0, 0, 0} },
    { MO_TURN, "(b)(i) Lengths multiply, so never expand.",
      "\\left|(2-i)^2(4+6i)\\right|=|2-i|^2\\,|4+6i|=5\\sqrt{52}=10\\sqrt{13}", NULL, "2.28", SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
    { MO_MIRROR, "(b)(ii) Compare $i+2$ with $i-2$.",
      "i-2=-\\overline{(2+i)}\\implies|i-2|=|2+i|\\implies\\left|\\frac{i+2}{i-2}\\right|^{57}=1",
      "A mirror image followed by a half turn has the same length.", NULL, SC_PLANE, {PL_CONJ, 0, 0, 0} },
    { MO_TURN, "(b)(iii) Only the real part of an exponent stretches.",
      "\\left|(2+3i)\\,e^{2+i}\\right|=\\sqrt{13}\\,e^2\\,|e^{i}|=\\sqrt{13}\\,e^2", "$|e^{a+ib}|=e^a$.", "2.37", SC_EXP, {2, 0, 0, 0} },
};
static const Step S_P2_4[] = {
    { MO_MIRROR, "(a) Expand the square through the mirror.",
      "|z+w|^2=(z+w)\\overline{(z+w)}=|z|^2+2\\Re(z\\overline w)+|w|^2", NULL, "2.32", SC_PLANE, {PL_TRIANGLE, 0, 0, 0} },
    { MO_SHADOW, "Shadow against length.",
      "\\le|z|^2+2|z||w|+|w|^2=(|z|+|w|)^2", NULL, NULL, SC_PLANE, {PL_SHADOW, 0, 0, 0} },
    { MO_GROUND, "(b) Write $z$ as a detour through $w$.",
      "z=(z-w)+w\\implies|z|-|w|\\le|z-w|", NULL, "2.33", SC_PLANE, {PL_TRIANGLE, 1, 0, 0} },
    { MO_MIRROR, "Swap $z$ and $w$.",
      "|w|-|z|\\le|z-w|\\implies\\bigl||z|-|w|\\bigr|\\le|z-w|",
      "This argument uses only the triangle inequality, so it works in any normed space (Problem 4.1).", NULL, KEEP },
};
static const Step S_P2_6[] = {
    { MO_SHADOW, "($\\Rightarrow$) Each lane is a shadow of the difference.",
      "|\\Re z_n-\\Re z|=|\\Re(z_n-z)|\\le|z_n-z|\\to0", "Likewise for $\\Im$.", NULL, SC_LANES, {1, 0, 0, 0} },
    { MO_SHADOW, "($\\Leftarrow$) A length is at most the sum of its lane lengths.",
      "|z_n-z|=\\sqrt{x_n^2+y_n^2}\\le|x_n|+|y_n|\\to0,\\qquad x_n=\\Re(z_n-z),\\ y_n=\\Im(z_n-z)",
      "Square both sides: $x^2+y^2\\le x^2+2|x||y|+y^2$.", NULL, KEEP },
    { MO_LANES, "Conclude.",
      "z_n\\to z\\iff\\Re z_n\\to\\Re z\\ \\text{and}\\ \\Im z_n\\to\\Im z", NULL, NULL, KEEP },
};
static const Step S_P2_7[] = {
    { MO_TURN, "Write both factors in polar form and use (2.38).",
      "z_1z_2=r_1e^{i\\theta_1}\\,r_2e^{i\\theta_2}=r_1r_2\\,e^{i(\\theta_1+\\theta_2)}", NULL, "2.38", SC_PLANE, {PL_PRODUCT, 0, 0, 0} },
    { MO_TURN, "Say it in one sentence.",
      "\\text{multiply the lengths, add the angles}",
      "Multiplying by $z_2$ stretches the whole plane by $|z_2|$ and turns it by $\\arg z_2$. Every complex product is a turn-and-stretch; drag the points to watch the triangles stay similar.", NULL, KEEP },
};
static const Step S_P2_9a[] = {
    { MO_SHADOW, "Throw away the good intervals. What is left of $U-L$?",
      "\\eta>U(g;P)-L(g;P)=\\sum_j(M_j-m_j)\\Delta x_j\\ge\\sum_{j\\in\\mathcal B_\\delta}(M_j-m_j)\\Delta x_j",
      "Every term is $\\ge0$.", NULL, SC_RIEMANN, {1, 0, 0, 0} },
    { MO_SHADOW, "On a bad interval the fluctuation is at least $\\delta$.",
      "\\ge\\delta\\sum_{j\\in\\mathcal B_\\delta}\\Delta x_j\\implies\\sum_{j\\in\\mathcal B_\\delta}\\Delta x_j\\le\\frac\\eta\\delta",
      "Little total fluctuation leaves little room for big fluctuations.", NULL, KEEP },
};
static const Step S_P2_9b[] = {
    { MO_GROUND, "Uniform continuity of $\\phi$ on the compact interval $[c,d]$.",
      "|y_1-y_2|\\le\\delta\\implies|\\phi(y_1)-\\phi(y_2)|<\\varepsilon", NULL, NULL, SC_RIEMANN, {1, 0, 0, 0} },
    { MO_SHADOW, "Good intervals: $g$ moves less than $\\delta$, so $\\phi\\circ g$ moves less than $\\varepsilon$.",
      "\\sum_{\\text{good}}\\bigl(M_j(\\phi\\circ g)-m_j(\\phi\\circ g)\\bigr)\\Delta x_j\\le\\varepsilon\\,(b-a)", NULL, NULL, KEEP },
    { MO_SHADOW, "Bad intervals: a crude bound, but they are short.",
      "\\sum_{\\text{bad}}\\bigl(M_j-m_j\\bigr)\\Delta x_j\\le2\\|\\phi\\circ g\\|_\\infty\\,\\frac\\eta\\delta", "Using (2.73) and part (a).", "P2.9a", KEEP },
    { MO_GROUND, "Choose $\\eta$.",
      "\\eta=\\frac{\\varepsilon\\,\\delta}{2\\|\\phi\\circ g\\|_\\infty+1}\\implies U-L\\le\\varepsilon\\,(b-a+1)",
      "$\\varepsilon$ was arbitrary, so rescale it to get $U-L<\\varepsilon$.", NULL, KEEP },
};
static const Step S_P2_11a[] = {
    { MO_GROUND, "Base case $n=0$.",
      "\\sum_{k=0}^{0}z^k=1=\\frac{1-z}{1-z}", NULL, NULL, SC_WINDING, {1, 6, 0, 0} },
    { MO_GROUND, "Induction step: add the next term.",
      "\\frac{1-z^{n+1}}{1-z}+z^{n+1}=\\frac{1-z^{n+1}+z^{n+1}-z^{n+2}}{1-z}=\\frac{1-z^{n+2}}{1-z}",
      "Pure field algebra, exactly as in $\\R$.", NULL, KEEP },
    { MO_TURN, "Picture the terms.",
      "1,\\ z,\\ z^2,\\ z^3,\\ \\dots",
      "Each term is the previous one turned by $\\arg z$ and scaled by $|z|$, so the partial sums trace a spiral path.", "P2.7", KEEP },
};
static const Step S_P2_11b[] = {
    { MO_LOOP, "Which $z$? $t\\notin\\Z$ guarantees $z\\ne1$.",
      "z=e^{2\\pi it}:\\qquad\\sum_{k=0}^Ne^{2\\pi ikt}=\\frac{1-e^{2\\pi i(N+1)t}}{1-e^{2\\pi it}}", NULL, "P2.11a", SC_WINDING, {2, 7, 0, 0} },
    { MO_SPLIT, "$1-e^{i\\varphi}$: factor out the half-angle, the middle of $0$ and $\\varphi$.",
      "1-e^{i\\varphi}=e^{i\\varphi/2}\\left(e^{-i\\varphi/2}-e^{i\\varphi/2}\\right)",
      "The same move as in (2.1): factor out the middle angle, leaving a symmetric pair.", "2.1", KEEP },
    { MO_MIRROR, "A unit vector minus its mirror image.",
      "e^{-i\\varphi/2}-e^{i\\varphi/2}=-2i\\sin(\\varphi/2)", NULL, "2.25", KEEP },
    { MO_TURN, "Do this above ($\\varphi=2\\pi(N+1)t$) and below ($\\varphi=2\\pi t$), then cancel.",
      "\\frac{e^{i\\pi(N+1)t}\\,(-2i)\\sin(\\pi(N+1)t)}{e^{i\\pi t}\\,(-2i)\\sin(\\pi t)}=e^{i\\pi Nt}\\,\\frac{\\sin(\\pi(N+1)t)}{\\sin(\\pi t)}", NULL, "2.38", KEEP },
    { MO_LANES, "Read off the lanes for (2.77).",
      "\\sum_{k=0}^N\\cos(2\\pi kt)=\\cos(\\pi Nt)\\frac{\\sin(\\pi(N+1)t)}{\\sin\\pi t},\\quad\\sum_{k=0}^N\\sin(2\\pi kt)=\\sin(\\pi Nt)\\frac{\\sin(\\pi(N+1)t)}{\\sin\\pi t}",
      "The ratio of sines is real, so $\\Re$ and $\\Im$ fall on $e^{i\\pi Nt}$ alone.", NULL, KEEP },
};
static const Step S_P2_12a[] = {
    { MO_GROUND, "What should an oscillation measure?",
      "\\omega(f;I):=\\sup_{x,y\\in I}|f(x)-f(y)|", "How much $f$ varies on $I$.", NULL, SC_RIEMANN, {0, 0, 0, 0} },
    { MO_SHADOW, "For real $f$: the largest gap between two values.",
      "\\sup_{x,y\\in I}|f(x)-f(y)|=\\sup_If-\\inf_If",
      "$f(x)-f(y)\\le\\sup f-\\inf f$, and taking $x,y$ near where the sup and inf are approached gets arbitrarily close.", NULL, KEEP },
    { MO_GROUND, "So the oscillation sum is exactly $U-L$.",
      "\\sum_j\\omega(f;[x_j,x_{j+1}])\\,\\Delta x_j=U(f;P)-L(f;P)", "and (2.80) is the criterion (2.70) in disguise.", NULL, KEEP },
};
static const Step S_P2_12b[] = {
    { MO_SHADOW, "Each lane oscillates no more than $f$ does.",
      "|u(x)-u(y)|=|\\Re(f(x)-f(y))|\\le|f(x)-f(y)|\\implies\\omega(u;I)\\le\\omega(f;I)", NULL, NULL, SC_RIEMANN, {3, 0, 0, 0} },
    { MO_SHADOW, "And $f$ no more than both lanes together.",
      "\\omega(f;I)\\le\\omega(u;I)+\\omega(v;I)", "Triangle inequality for $f(x)-f(y)=(u(x)-u(y))+i(v(x)-v(y))$.", "2.32", KEEP },
    { MO_LANES, "Conclude both directions.",
      "f\\ \\text{integrable in the sense of Def. 2.7}\\iff u,v\\in\\mathcal R",
      "($\\Rightarrow$) Step 1 with the same partition. ($\\Leftarrow$) Take a common refinement of good partitions for $u$ and $v$ (refining never increases an oscillation sum), then step 2.", NULL, KEEP },
};

/* ================================================================ pieces */
#define NONE NULL, 0
const Piece PIECES[] = {
    /* ---- 1.1 */
    { "1.1", "(1.1)", "The wave equation", 1, 0, K_GROUND, 0,
      "\\frac{\\partial^2 y}{\\partial x^2}=\\frac{1}{c^2}\\frac{\\partial^2 y}{\\partial t^2}",
      "How bent the string is at a point (curvature in $x$) decides how fast that point accelerates (in $t$). The constant $c$ has units of speed, and the analysis shows it *is* the speed of the travelling waves.",
      {0}, NONE, SC_STRING, {0, 0, 0, 0},
      "Given by physics: the continuum limit of a chain of tiny harmonic oscillators. Ground.", NULL },
    /* ---- 1.2 */
    { "1.2", "(1.2)", "Nailed ends", 1, 1, K_GROUND, 0,
      "y(0,t)=0,\\qquad y(\\ell,t)=0\\qquad\\text{for all }t",
      "The string is fixed at both ends. With (1.1) this is a boundary value problem. As the derivation will show, the nails are mirrors.",
      {0}, NONE, SC_STRING, {2, 0, 0, 0}, "The physical set-up. Ground.", NULL },
    { "1.3", "(1.3)", "Clairaut-Schwarz", 1, 1, K_GROUND, 0,
      "\\frac{\\partial^2 y}{\\partial x\\,\\partial t}=\\frac{\\partial^2 y}{\\partial t\\,\\partial x}\\qquad(y\\in C^2)",
      "For $C^2$ functions the order of partial derivatives doesn't matter: $\\partial_x$ and $\\partial_t$ commute, just as real numbers do. That is all the factorisation (1.24) needs.",
      {0}, NONE, SC_SPACETIME, {1, 0, 0, 0}, "Multivariable calculus. Ground.", NULL },
    { "1.6", "(1.6)", "Characteristic coordinates", 1, 1, K_RESULT, 0,
      "u=x+ct,\\qquad v=x-ct",
      "The new axes lie along the two diagonals of the $(x,ct)$-plane, the directions along which the factored operator differentiates.",
      {"1.24", 0}, NS(S_1_6), SC_SPACETIME, {0, 0, 0, 0}, NULL,
      "Split returns as $\\gamma,\\delta$ in (2.1) and in Problem 2.11(b). Turn returns in Problem 2.7: the change of coordinates is multiplication by $1+i$." },
    { "L1.1", "Lemma 1.1", "The wave equation in u, v", 1, 1, K_RESULT, 0,
      "\\Box y=0\\iff\\frac{\\partial^2 y}{\\partial u\\,\\partial v}=0",
      "In characteristic coordinates the wave operator is just $4\\,\\partial_u\\partial_v$.",
      {"1.24", "1.26", 0}, NS(S_L1_1), SC_SPACETIME, {1, 0, 0, 0},
      "Erratum: the book's proof writes the first factor as $\\{\\tfrac1c\\partial_t+\\partial_x\\}\\{\\tfrac1c\\partial_t-\\partial_x\\}$, which equals $-\\Box$. The sign is harmless when $\\Box y=0$; the correct identity is $\\Box=4\\,\\partial_u\\partial_v$.", NULL },
    { "1.14", "Theorem 1.4 (i)", "Every solution is two travelling waves", 1, 1, K_RESULT, 0,
      "y(x,t)=f(x+ct)+g(x-ct)",
      "Two shapes, one sliding left and one sliding right at speed $c$, passing through each other without interacting.",
      {"L1.1", 0}, NS(S_1_14), SC_STRING, {0, 0, 0, 0}, NULL,
      "The Hold move returns in separation of variables (Problem 1.3a): a function of $x$ equal to a function of $t$ is a constant." },
    { "1.12", "(1.12)", "The first nail is a mirror", 1, 1, K_RESULT, 0,
      "y=f(x+ct)-f(-x+ct)",
      "The fixed end at $0$ turns the right-moving wave into the inverted mirror image of the left-moving one.",
      {"1.14", "1.2", 0}, NS(S_1_12), SC_STRING, {1, 0, 0, 0}, NULL,
      "The Mirror returns as complex conjugation $z\\mapsto\\overline z$ in Chapter 2, where again a thing plus its mirror image is simpler than either." },
    { "1.13", "(1.13)", "Two mirrors make a loop", 1, 1, K_RESULT, 0,
      "f(\\lambda+2\\ell)=f(\\lambda)\\qquad\\text{for all }\\lambda",
      "The second nail forces $f$ to be $2\\ell$-periodic. Periodicity, the signature of a tone, is forced by the geometry of the instrument.",
      {"1.12", "1.2", 0}, NS(S_1_13), SC_STRING, {3, 0, 0, 0}, NULL,
      "This is the whole reason a string makes tones and a door-knock does not." },
    { "D1.2", "Definition 1.2", "Periods and the minimal period", 1, 1, K_DEF, 0,
      "f(t+T)=f(t)\\ \\ \\forall t;\\qquad T_f=\\min\\bigl(\\Pi_f\\cap(0,\\infty)\\bigr)",
      "A period is a shift that changes nothing. Periods are never unique ($2T$, $-T$, ...), and a smallest positive one may or may not exist: constant functions have every period.",
      {0}, NONE, SC_PERIODS, {0, 0, 0, 0}, NULL, NULL },
    { "Pr1.2.1", "Proposition 1.2.1", "When a minimal period exists", 1, 1, K_RESULT, 0,
      "f\\ \\text{periodic, not constant, continuous at one point}\\implies T_f\\ \\text{exists}",
      "Musically, the minimal period is the pitch (Section 7.1). The proof is Problem 1.1.",
      {"D1.2", 0}, NS(S_P1_2_1), SC_PERIODS, {0, 0, 0, 0}, NULL, NULL },
    { "T1.4", "Theorem 1.4", "d'Alembert's solution", 1, 1, K_RESULT, 0,
      "y=f(x+ct)-f(-x+ct),\\qquad f(\\lambda+2\\ell)=f(\\lambda)",
      "The vibrating string is two copies of one $2\\ell$-periodic shape travelling in opposite directions, one of them inverted.",
      {"1.14", "1.12", "1.13", 0}, NS(S_T1_4), SC_STRING, {2, 0, 0, 0}, NULL, NULL },
    { "1.17", "(1.17)", "The simplest loops", 1, 1, K_RESULT, 0,
      "f(\\lambda)=K\\sin\\left(\\frac{n\\pi\\lambda}{\\ell}+\\phi\\right),\\quad n\\in\\N",
      "The sines that make a whole number of turns in one period $2\\ell$.",
      {"1.13", 0}, NS(S_1_17), SC_STANDING, {1, 0, 1, 0}, NULL, NULL },
    { "1.18", "(1.18)", "Bernoulli's standing waves", 1, 1, K_RESULT, 0,
      "y(x,t)=2K\\sin\\left(\\frac{n\\pi x}{\\ell}\\right)\\cos\\left(\\frac{n\\pi ct}{\\ell}+\\phi\\right)",
      "Two counter-travelling sines add up to a fixed shape that only breathes in time. Here derived the Chapter 2 way; Problem 1.2 does it with trigonometry.",
      {"1.17", "T1.4", 0}, NS(S_1_18), SC_STANDING, {2, 0, 1, 0}, NULL,
      "This is the beats identity (2.1) with space and time trading places. In beats the half-gap oscillates slowly in *time*, so loudness swells. Here it is frozen in *space*, which gives the nodes." },
    { "1.19", "(1.19)-(1.22)", "The harmonic series", 1, 1, K_RESULT, 0,
      "\\nu_n=\\frac{nc}{2\\ell}=n\\,\\nu_1,\\qquad \\lambda_n=\\frac{2\\ell}{n}",
      "A string can only ring at whole multiples of its fundamental $\\nu_1=c/2\\ell$. The overtones $n\\ge2$ colour the sound (timbre, Chapter 3).",
      {"1.18", 0}, NS(S_1_19), SC_STANDING, {3, 0, 0, 0}, NULL,
      "In this score each motif sounds one of these harmonics, and the ground is the fundamental." },
    { "1.24", "(1.24)", "Factoring the wave operator", 1, 1, K_RESULT, 0,
      "\\Box=\\left(\\partial_x+\\tfrac1c\\partial_t\\right)\\left(\\partial_x-\\tfrac1c\\partial_t\\right)",
      "$A^2-B^2=(A+B)(A-B)$ works for operators that commute.",
      {"1.1", "1.3", 0}, NS(S_1_24), SC_SPACETIME, {1, 0, 0, 0}, NULL,
      "The same factorisation returns in (2.31): $(a+ib)(a-ib)=a^2+b^2$ is $A^2-B^2$ with $B=ib$." },
    { "1.26", "(1.26)", "The chain rule in u, v", 1, 1, K_RESULT, 0,
      "\\partial_x=\\partial_u+\\partial_v,\\qquad \\partial_t=c\\,\\partial_u-c\\,\\partial_v",
      "Old derivatives in terms of new ones; the factors of $\\Box$ become $2\\partial_u$ and $2\\partial_v$.",
      {"1.6", 0}, NS(S_1_26), SC_SPACETIME, {1, 0, 0, 0}, NULL, NULL },
    /* ---- 1.3 problems */
    { "P1.1a", "Problem 1.1(a)", "Periods add and subtract", 1, 2, K_PROBLEM, 1,
      "T_1,T_2\\in\\Pi_f\\cup\\{0\\}\\implies T_1+T_2,\\ -T_1\\in\\Pi_f\\cup\\{0\\}",
      "Two loops in a row are a loop; a loop backwards is a loop.", {"D1.2", 0}, NS(S_P1_1a), SC_PERIODS, {1, 0, 0, 0}, NULL, NULL },
    { "P1.1b", "Problem 1.1(b)", "No crowding at 0, no crowding anywhere", 1, 2, K_PROBLEM, 1,
      "0\\ \\text{not a limit point of}\\ \\Pi_f\\implies T_f\\ \\text{exists}",
      "Because periods form a group, any crowding can be moved to $0$ by subtracting.", {"P1.1a", 0}, NS(S_P1_1b), SC_PERIODS, {0, 0, 0, 0}, NULL, NULL },
    { "P1.1c", "Problem 1.1(c)", "Continuity forbids tiny periods", 1, 2, K_PROBLEM, 1,
      "f\\ \\text{continuous at}\\ t_0,\\ \\text{not constant}\\implies 0\\ \\text{is not a limit point of}\\ \\Pi_f",
      "Arbitrarily small periods carry the value at $t_0$ everywhere.", {"P1.1a", "P1.1b", 0}, NS(S_P1_1c), SC_PERIODS, {2, 0, 0, 0}, NULL, NULL },
    { "P1.2", "Problem 1.2", "Bernoulli by trigonometry", 1, 2, K_PROBLEM, 0,
      "f(\\lambda)=K\\sin\\left(\\tfrac{n\\pi\\lambda}{\\ell}+\\phi\\right)\\implies y=2K\\sin\\left(\\tfrac{n\\pi x}{\\ell}\\right)\\cos\\left(\\tfrac{n\\pi ct}{\\ell}+\\phi\\right)",
      "The book's route via the sum-angle formula. Compare with the complex route in (1.18): same Split, same Mirror.", {"T1.4", "1.17", 0}, NS(S_P1_2), SC_STANDING, {2, 0, 1, 0}, NULL,
      "Same chord as (2.1): Split, Turn, Mirror." },
    { "P1.3a", "Problem 1.3(a)", "Separation of variables", 1, 2, K_PROBLEM, 0,
      "y=f(x)g(t)\\implies \\frac{f''(x)}{f(x)}=\\frac{1}{c^2}\\frac{g''(t)}{g(t)}=-k",
      "Look for product solutions; the PDE splits into two ODEs.", {"1.1", 0}, NS(S_P1_3a), SC_SEPARATION, {4, 0, 0, 0}, NULL,
      "The same Hold move as $\\partial_u(\\partial_v y)=0\\Rightarrow\\partial_vy=h(v)$ in Theorem 1.4." },
    { "P1.3b", "Problem 1.3(b)", "Nails on a product", 1, 2, K_PROBLEM, 0,
      "f(0)=f(\\ell)=0", "The boundary conditions land entirely on the space factor.", {"1.2", "P1.3a", 0}, NS(S_P1_3b), SC_SEPARATION, {4, 0, 0, 0}, NULL, NULL },
    { "P1.3c", "Problem 1.3(c)", "Only whole half-turns fit", 1, 2, K_PROBLEM, 0,
      "f''=-kf,\\ \\ f(0)=f(\\ell)=0\\ \\text{has}\\ f\\not\\equiv0\\iff k=\\left(\\frac{n\\pi}{\\ell}\\right)^2",
      "Drag $k$ on the stage: only at $k_n$ does the curve land on the far nail.", {"P1.3a", "P1.3b", 0}, NS(S_P1_3c), SC_SEPARATION, {2.3f, 0, 0, 0},
      "Erratum: the book writes $\\nu_n=\\frac1{2\\pi}\\sqrt{k_n}$ in the middle; a factor $c$ is missing, since the time equation is $g''=-kc^2g$.", NULL },
    { "P1.3d", "Problem 1.3(d)", "The product solutions are Bernoulli's", 1, 2, K_PROBLEM, 0,
      "y=C\\sin\\left(\\frac{n\\pi x}{\\ell}\\right)\\sin\\left(\\frac{n\\pi ct}{\\ell}+\\phi\\right)",
      "Solve the time equation and merge cosine and sine into one shifted sine.", {"P1.3c", 0}, NS(S_P1_3d), SC_PLANE, {PL_POLAR, 0, 0, 0},
      "Erratum: the hint's $\\phi=\\arctan(A/B)$ is right only when $B>0$. In general $\\phi=\\arg(B+iA)$, because the quadrant matters.", NULL },
    { "P1.4", "Problem 1.4", "Dividing the string", 1, 2, K_PROBLEM, 0,
      "\\text{touch at}\\ \\tfrac{\\ell}{N}\\implies\\text{only}\\ n\\in N\\N\\ \\text{survive; pitch}\\ \\nu_N",
      "A light finger is a node you impose; only modes that already have a node there survive.", {"1.19", 0}, NS(S_P1_4), SC_STANDING, {4, 3, 0, 0}, NULL, NULL },

    /* ---- 2.1 */
    { "2.1", "(2.1)", "Beats: a sum of turns is a product", 2, 3, K_RESULT, 0,
      "\\sin\\alpha+\\sin\\beta=2\\cos\\left(\\frac{\\alpha-\\beta}{2}\\right)\\sin\\left(\\frac{\\alpha+\\beta}{2}\\right)",
      "Proved in §2.3 by complexification (2.40)-(2.41). Drag the two arrows: their sum always points along the middle angle, with length $2|\\cos\\delta|$.",
      {"2.36", "2.38", "2.25", 0}, NS(S_2_1), SC_PHASORS, {1, 0, 0, 0}, NULL,
      "Bernoulli (1.18) is the same identity with $x$ and $t$ exchanged. Problem 2.11(b) uses the same middle-angle factoring." },
    { "Beats", "Experiment 2.1.1", "Hearing beats", 2, 3, K_RESULT, 0,
      "\\sin(2\\pi\\nu_1t)+\\sin(2\\pi\\nu_0t)=2\\cos\\bigl(\\pi(\\nu_1-\\nu_0)t\\bigr)\\sin\\bigl(\\pi(\\nu_0+\\nu_1)t\\bigr)",
      "Two nearby tones sound like their average, swelling and fading $\\nu_1-\\nu_0$ times per second. Musicians tune by listening for these beats to slow down and stop.",
      {"2.1", 0}, NS(S_BEATS), SC_PHASORS, {0, 2, 0, 0}, NULL, NULL },
    { "2.3", "(2.3)-(2.4)", "Sum-angle formulas", 2, 3, K_GROUND, 0,
      "\\sin(a+b)=\\sin a\\cos b+\\sin b\\cos a,\\quad \\cos(a+b)=\\cos a\\cos b-\\sin a\\sin b",
      "Trigonometry, taken as given. In the language of §2.3 they say one thing: turning by $a$ and then by $b$ is turning by $a+b$.",
      {0}, NONE, SC_EXP, {3, 0, 0, 0},
      "Equivalent to (2.38) for imaginary arguments. The book proves (2.38) from these; reading (2.38) in both lanes gives them back.", NULL },
    /* ---- 2.2 */
    { "2.5", "(2.5)-(2.6)", "The plane as a vector space", 2, 4, K_GROUND, 0,
      "\\pmat{a}{b}+\\pmat{c}{d}=\\pmat{a+c}{b+d},\\qquad \\alpha\\pmat{a}{b}=\\pmat{\\alpha a}{\\alpha b}",
      "Parallelogram addition and stretching. $\\C$ begins as this plane.", {0}, NONE, SC_PLANE, {PL_SUM, 0, 0, 0}, NULL, NULL },
    { "2.9", "(2.7)-(2.11)", "Real and imaginary lanes", 2, 4, K_DEF, 0,
      "z=a\\,\\hat1+b\\,\\hat\\imath,\\qquad \\Re(z+w)=\\Re z+\\Re w,\\ \\ \\Im(z+w)=\\Im z+\\Im w",
      "Every point has two coordinates, its lanes, and addition happens in each lane separately.", {"2.5", 0}, NONE, SC_LANES, {0, 0, 0, 0}, NULL, NULL },
    { "2.12", "(2.12)", "The product", 2, 4, K_DEF, 0,
      "\\pmat{a}{b}\\cdot\\pmat{c}{d}:=\\pmat{ac-bd}{ad+bc}",
      "The book's definition. The steps run the logic backwards: why is *this* the right product?",
      {"2.5", 0}, NS(S_2_12), SC_PLANE, {PL_ITURN, 0, 0, 0},
      "The book takes (2.12) as the definition and derives (2.15)-(2.17). Here we start from \"$\\hat\\imath$ is a quarter turn\" and arrive at (2.12). Both directions are valid.", NULL },
    { "2.16", "(2.15)-(2.17)", "i squared", 2, 4, K_RESULT, 0,
      "\\hat\\imath\\cdot\\hat\\imath=-\\hat1,\\qquad \\hat1\\cdot\\hat1=\\hat1,\\qquad \\hat1\\cdot\\hat\\imath=\\hat\\imath",
      "The most famous identity in $\\C$, as a consequence of (2.12).", {"2.12", 0}, NS(S_2_16), SC_PLANE, {PL_ITURN, 0, 0, 0}, NULL, NULL },
    { "2.18", "(2.18)-(2.19)", "Division is possible", 2, 4, K_RESULT, 0,
      "z^{-1}=\\frac{a-ib}{a^2+b^2}=\\frac{\\overline z}{|z|^2},\\qquad z^{-1}z=1",
      "Every non-zero $z$ has an inverse, which makes $\\C$ a field.", {"2.31", 0}, NS(S_2_18), SC_PLANE, {PL_INVERSE, 0, 0, 0}, NULL, NULL },
    { "2.21", "(2.20)-(2.22)", "R inside C", 2, 4, K_DEF, 0,
      "x\\mapsto(x,0),\\qquad (\\alpha,0)\\cdot z=\\alpha z,\\qquad z=a+ib",
      "The real line sits in $\\C$ as the real axis, products with reals are stretchings, and the product distributes over sums.",
      {"2.12", 0}, NONE, SC_LANES, {0, 0, 0, 0}, NULL, NULL },
    /* ---- 2.3 */
    { "2.23", "(2.23)", "Term-by-term multiplication", 2, 5, K_RESULT, 0,
      "(a+ib)(c+id)=(ac-bd)+i(ad+bc)", "Distribute, then use $i^2=-1$.", {"2.16", "2.21", 0}, NS(S_2_23), SC_PLANE, {PL_PRODUCT, 0, 0, 0}, NULL, NULL },
    { "2.24", "(2.24)", "The conjugate", 2, 5, K_DEF, 0,
      "\\overline z=a-ib", "Reflection in the real axis: Chapter 2's mirror.", {"2.21", 0}, NONE, SC_PLANE, {PL_CONJ, 0, 0, 0}, NULL,
      "The nails of Chapter 1 were mirrors too (1.12)." },
    { "2.25", "(2.25)-(2.26)", "Lanes from the mirror", 2, 5, K_RESULT, 0,
      "\\Re z=\\frac{z+\\overline z}{2},\\qquad \\Im z=\\frac{z-\\overline z}{2i}",
      "Coordinate-free real and imaginary parts.", {"2.24", 0}, NS(S_2_25), SC_PLANE, {PL_REIM, 0, 0, 0}, NULL,
      "Used in (2.1), in the triangle inequality and in Problem 2.11: whenever a thing meets its mirror image." },
    { "2.27", "(2.27)", "Absolute value", 2, 5, K_DEF, 0,
      "|z|=\\sqrt{a^2+b^2}", "Euclidean length. For real $z$ it is the usual absolute value.", {"2.21", 0}, NONE, SC_PLANE, {PL_MODSQ, 0, 0, 0}, NULL, NULL },
    { "2.31", "(2.31)", "Length through the mirror", 2, 5, K_RESULT, 0,
      "|z|^2=z\\,\\overline z", "The coordinate-free length, and the key to every computation with $|\\cdot|$.", {"2.24", "2.27", 0}, NS(S_2_31), SC_PLANE, {PL_MODSQ, 0, 0, 0}, NULL,
      "The analogue of $\\|v\\|^2=\\langle v,v\\rangle$; it returns as the inner product of Chapter 4." },
    { "2.28", "(2.28)-(2.30)", "Lengths multiply", 2, 5, K_RESULT, 0,
      "|zw|=|z||w|,\\qquad\\left|\\frac zw\\right|=\\frac{|z|}{|w|},\\qquad|\\overline z|=|z|",
      "The absolute value distributes over products and quotients; the mirror preserves it.", {"2.31", 0}, NS(S_2_28), SC_PLANE, {PL_PRODUCT, 0, 0, 0}, NULL, NULL },
    { "2.32", "Proposition 2.3.1 (i)", "Triangle inequality", 2, 5, K_RESULT, 0,
      "|z+w|\\le|z|+|w|", "Here proved the purely complex way (Problem 2.4a).", {"2.31", "2.28", 0}, NS(S_2_32), SC_PLANE, {PL_TRIANGLE, 0, 0, 0}, NULL,
      "The same Shadow move proves the integral version (Theorem 2.5), with a Turn first." },
    { "2.33", "Proposition 2.3.1 (ii)", "Reverse triangle inequality", 2, 5, K_RESULT, 0,
      "\\bigl||z|-|w|\\bigr|\\le|z-w|", "Lengths can't differ by more than the distance between the points.", {"2.32", 0}, NS(S_2_33), SC_PLANE, {PL_TRIANGLE, 1, 0, 0}, NULL, NULL },
    { "2.34", "(2.34)", "Dividing in practice", 2, 5, K_RESULT, 0,
      "\\frac{a+ib}{c+id}=\\frac{ac+bd}{c^2+d^2}+i\\,\\frac{bc-ad}{c^2+d^2}", "Multiply above and below by the mirror image of the denominator.", {"2.31", "2.23", 0}, NS(S_2_34), SC_PLANE, {PL_DIVIDE, 0, 0, 0}, NULL, NULL },
    { "2.36", "(2.36)", "The unit turn", 2, 5, K_DEF, 0,
      "e^{i\\theta}:=\\cos\\theta+i\\sin\\theta", "For now just a name for the unit vector at angle $\\theta$. (2.38) and (2.47) justify the name: it behaves exactly like an exponential.",
      {0}, NONE, SC_EXP, {0, 0, 0, 0}, NULL, NULL },
    { "2.37", "(2.37)", "The complex exponential", 2, 5, K_DEF, 0,
      "e^{a+ib}:=e^a\\left(\\cos b+i\\sin b\\right)", "The real part of the exponent stretches, the imaginary part turns.", {"2.36", 0}, NONE, SC_EXP, {2, 0, 0, 0}, NULL, NULL },
    { "2.35", "(2.35), (2.39)", "Polar form", 2, 5, K_RESULT, 0,
      "z=r(\\cos\\theta+i\\sin\\theta)=re^{i\\theta}", "Every non-zero $z$ is a length times a direction.", {"2.36", "2.27", 0}, NS(S_2_35), SC_PLANE, {PL_POLAR, 0, 0, 0}, NULL, NULL },
    { "2.38", "(2.38)", "Turns add: the functional equation", 2, 5, K_RESULT, 1,
      "e^{z+w}=e^z\\,e^w", "The key property of the exponential survives in $\\C$, and it is far easier to remember than the trigonometric identities it contains.",
      {"2.37", "2.3", 0}, NS(S_2_38), SC_EXP, {3, 0, 0, 0},
      "Logic: the book proves (2.38) from (2.3)-(2.4). Conversely, (2.38) for imaginary arguments, read in both lanes, *is* (2.3)-(2.4). They are one fact: rotations add angles.", NULL },
    /* ---- 2.4 */
    { "An", "§2.4 (1)-(4)", "Analysis transfers", 2, 6, K_GROUND, 0,
      "|z_n-z|\\to0,\\quad \\text{Cauchy},\\quad \\text{open, closed, compact}\\ \\ \\text{as in}\\ \\R^2",
      "Because $|z|$ is Euclidean length, analysis in $\\R^2$ (triangle inequality, convergence, Cauchy criterion, Heine-Borel) *is* analysis in $\\C$.", {"2.27", 0}, NONE, SC_LANES, {1, 0, 0, 0}, NULL, NULL },
    { "2.42", "(2.42)", "The circle traced", 2, 6, K_RESULT, 0,
      "t\\mapsto e^{it}=\\cos t+i\\sin t", "A complex-valued function of a real variable: the prototype of a tone.", {"2.36", 0}, NS(S_2_42), SC_EXP, {0, 0, 0, 0}, NULL, NULL },
    { "2.43", "(2.43)-(2.44)", "Calculus lane by lane", 2, 6, K_DEF, 0,
      "f'=u'+iv',\\qquad \\int_a^bf=\\int_a^bu+i\\int_a^bv",
      "Define derivative and integral in each lane; every real theorem about them comes along for free.", {"2.9", 0}, NONE, SC_LANES, {2, 0, 0, 0}, NULL, NULL },
    { "2.45", "(2.45)", "Complex linearity of the integral", 2, 6, K_RESULT, 0,
      "\\int_a^b(\\alpha f+\\beta g)=\\alpha\\int_a^bf+\\beta\\int_a^bg,\\qquad\\alpha,\\beta\\in\\C",
      "Complex constants come out of integrals.", {"2.43", "2.23", 0}, NS(S_2_45), SC_LANES, {3, 0, 0, 0}, NULL, NULL },
    { "FTC", "Theorems 2.1-2.3", "FTC in C", 2, 6, K_GROUND, 0,
      "f\\in C\\implies f\\in\\mathcal R,\\quad \\frac{d}{dx}\\int_a^xf=f,\\quad \\int_a^bf=G(b)-G(a)",
      "Each part holds lane by lane, so it holds in $\\C$.", {"2.43", 0}, NONE, SC_LANES, {3, 0, 0, 0},
      "Typo: Theorem 2.1 writes $\\int\\Re f+\\int\\Im f$; it should read $\\int\\Re f+i\\int\\Im f$.", NULL },
    { "2.47", "(2.47)", "Derivative of a spiral", 2, 6, K_RESULT, 1,
      "\\frac{d}{dt}e^{\\lambda t}=\\lambda\\,e^{\\lambda t}", "Velocity is position, turned and stretched by $\\lambda$.", {"2.37", "2.43", 0}, NS(S_2_47), SC_EXP, {1, 0, 0, 0}, NULL, NULL },
    { "2.48", "(2.48)", "Integral of a spiral", 2, 6, K_RESULT, 1,
      "\\int_a^be^{\\lambda t}\\,dt=\\frac{1}{\\lambda}\\left(e^{\\lambda b}-e^{\\lambda a}\\right),\\quad\\lambda\\ne0", "Exactly as for real exponentials.", {"2.47", "FTC", 0}, NS(S_2_48), SC_EXP, {1, 0, 0, 0}, NULL, NULL },
    { "2.50", "Example 2.4", "Orthonormality", 2, 6, K_RESULT, 0,
      "\\int_0^1\\overline{e_n(x)}\\,e_m(x)\\,dx=\\delta_{n,m},\\qquad e_n(x)=e^{2\\pi inx}",
      "Different whole-number windings are perpendicular. This will let us read off the harmonics of any tone (Fourier series, Chapter 3).",
      {"2.48", "2.24", 0}, NS(S_2_50), SC_WINDING, {0, 2, 0, 0}, NULL,
      "Chapter 1 showed a string can only ring at whole multiples $n\\nu_1$; this shows those multiples can be separated again." },
    { "2.51", "(2.51)-(2.52)", "The sup-norm", 2, 6, K_DEF, 0,
      "\\|f\\|_{\\infty}=\\sup_{t\\in[a,b]}|f(t)|,\\qquad f_n\\to f\\ \\text{uniformly}\\iff\\|f_n-f\\|_\\infty\\to0",
      "The largest length the function reaches. It measures uniform closeness.", {"2.27", 0}, NONE, SC_RIEMANN, {2, 0, 0, 0}, NULL, NULL },
    { "T2.5ii", "Theorem 2.5 (ii)", "Triangle inequality for integrals", 2, 6, K_RESULT, 0,
      "\\left|\\int_a^bf(t)\\,dt\\right|\\le\\int_a^b|f(t)|\\,dt",
      "Polarisation: turn the integral onto the real axis, then compare shadows with lengths.", {"2.35", "2.45", "T2.5i", 0}, NS(S_T2_5ii), SC_POLARIZE, {0, 0, 0, 0}, NULL,
      "A path made of little steps $f(t)\\,dt$ is at least as long as the straight line from its start to its end. Same Shadow as (2.32)." },
    { "Pr2.4.1", "Proposition 2.4.1", "Continuous of integrable is integrable", 2, 6, K_RESULT, 0,
      "g\\in\\mathcal R([a,b];[c,d]),\\ \\phi\\in C([c,d])\\implies\\phi\\circ g\\in\\mathcal R",
      "The proof is Problem 2.9.", {0}, NS(S_P2_4_1), SC_RIEMANN, {1, 0, 0, 0}, NULL, NULL },
    { "D2.6", "Definition 2.6", "Piecewise continuous", 2, 6, K_DEF, 0,
      "f\\in\\mathcal{PC}:\\ \\text{finitely many jumps},\\qquad \\int_a^bf=\\sum_j\\int_{t_j}^{t_{j+1}}f",
      "Continuous except for finitely many jumps with one-sided limits; integrate piece by piece.", {"2.43", 0}, NONE, SC_RIEMANN, {4, 0, 0, 0}, NULL, NULL },
    { "T2.5i", "Theorem 2.5 (i)", "The length of an integrable function is integrable", 2, 6, K_RESULT, 0,
      "f\\in\\mathcal R([a,b];\\C)\\implies|f|\\in\\mathcal R([a,b];\\R)",
      "$|f|$ is not linear in the lanes, so this needs an argument.", {"Pr2.4.1", "D2.6", 0}, NS(S_T2_5i), SC_LANES, {0, 0, 0, 0}, NULL, NULL },
    { "C2.4.1", "Corollary 2.4.1", "The M-L estimate", 2, 6, K_RESULT, 1,
      "\\left|\\int_Sf(t)\\,dt\\right|\\le\\|f\\|_{S;\\infty}\\cdot|S|",
      "An integral is at most (largest length) times (length of the domain). Step 1: find $M$. Step 2: find $L$.", {"T2.5ii", "2.51", 0}, NS(S_C2_4_1), SC_RIEMANN, {2, 0, 0, 0}, NULL, NULL },
    /* ---- 2.5 problems */
    { "P2.1", "Problem 2.1", "The field axioms", 2, 7, K_PROBLEM, 0,
      "zw=wz,\\quad z(wu)=(zw)u,\\quad z^{-1}z=1,\\quad (z_1+z_2)w=z_1w+z_2w", "Checks from (2.12), and what they look like.", {"2.12", 0}, NS(S_P2_1), SC_PLANE, {PL_PRODUCT, 0, 0, 0}, NULL, NULL },
    { "P2.2", "Problem 2.2", "Identities through the mirror", 2, 7, K_PROBLEM, 0,
      "\\Re z=\\tfrac{z+\\overline z}{2},\\ \\ |z|^2=z\\overline z,\\ \\ |zw|=|z||w|,\\ \\ |\\overline z|=|z|", "All four parts are Mirror moves.", {"2.24", 0}, NS(S_P2_2), SC_PLANE, {PL_CONJ, 0, 0, 0}, NULL, NULL },
    { "P2.3", "Problem 2.3", "Practice", 2, 7, K_PROBLEM, 0,
      "\\frac{i^2}{i^3-4i+6},\\quad e^{4(2+\\sqrt2 i)t},\\quad |(2-i)^2(4+6i)|,\\quad \\left|\\tfrac{i+2}{i-2}\\right|^{57},\\quad |(2+3i)e^{2+i}|",
      "Every part falls to one motif: Turn, Mirror or Lanes.", {"2.34", "2.28", "2.37", 0}, NS(S_P2_3), SC_PLANE, {PL_DIVIDE, 0, 0, 0}, NULL, NULL },
    { "P2.4", "Problem 2.4", "Triangle inequality, purely complex", 2, 7, K_PROBLEM, 0,
      "|z+w|\\le|z|+|w|,\\qquad \\bigl||z|-|w|\\bigr|\\le|z-w|", "Expand $|z+w|^2$ through the mirror.", {"2.31", 0}, NS(S_P2_4), SC_PLANE, {PL_TRIANGLE, 0, 0, 0}, NULL, NULL },
    { "P2.5", "Problem 2.5", "Prove the functional equation", 2, 7, K_PROBLEM, 1,
      "e^{z+w}=e^z\\,e^w", "This is (2.38).", {"2.37", "2.3", 0}, NS(S_2_38), SC_EXP, {3, 0, 0, 0}, NULL, NULL },
    { "P2.6", "Problem 2.6", "Convergence lane by lane", 2, 7, K_PROBLEM, 0,
      "z_n\\to z\\iff \\Re z_n\\to\\Re z\\ \\text{and}\\ \\Im z_n\\to\\Im z", "Two shadows and one triangle inequality.", {"2.27", 0}, NS(S_P2_6), SC_LANES, {1, 0, 0, 0}, NULL, NULL },
    { "P2.7", "Problem 2.7 / (2.68)", "Multiplication is turn and stretch", 2, 7, K_PROBLEM, 0,
      "z_1z_2=r_1r_2\\,e^{i(\\theta_1+\\theta_2)}", "The geometric meaning of the complex product.", {"2.38", "2.35", 0}, NS(S_P2_7), SC_PLANE, {PL_PRODUCT, 0, 0, 0}, NULL,
      "Even d'Alembert's coordinates (1.6) are such a product: $v+iu=(1+i)(x+ict)$." },
    { "P2.8", "Problem 2.8", "Calculus of the exponential", 2, 7, K_PROBLEM, 1,
      "\\frac{d}{dt}e^{\\lambda t}=\\lambda e^{\\lambda t},\\qquad \\int_a^be^{\\lambda t}dt=\\tfrac1\\lambda\\left(e^{\\lambda b}-e^{\\lambda a}\\right)", "First (2.47), then (2.48) by FTC-3.", {"2.37", "2.43", "FTC", 0}, NS(S_P2_8), SC_EXP, {1, 0, 0, 0}, NULL, NULL },
    { "P2.9a", "Problem 2.9(a)", "Few bad intervals", 2, 7, K_PROBLEM, 0,
      "\\sum_{j\\in\\mathcal B_\\delta}(x_{j+1}-x_j)\\le\\frac\\eta\\delta", "If the total fluctuation is small, the intervals with large fluctuation are short.", {0}, NS(S_P2_9a), SC_RIEMANN, {1, 0, 0, 0}, NULL, NULL },
    { "P2.9b", "Problem 2.9(b)", "Proof of Proposition 2.4.1", 2, 7, K_PROBLEM, 0,
      "U(\\phi\\circ g;P)-L(\\phi\\circ g;P)<\\varepsilon", "Good intervals by uniform continuity, bad ones by their small total length.", {"P2.9a", 0}, NS(S_P2_9b), SC_RIEMANN, {1, 0, 0, 0}, NULL, NULL },
    { "P2.10", "Problem 2.10", "Prove the M-L estimate", 2, 7, K_PROBLEM, 1,
      "\\left|\\int_Sf\\right|\\le\\|f\\|_{S;\\infty}\\cdot|S|", "This is Corollary 2.4.1.", {"T2.5ii", 0}, NS(S_C2_4_1), SC_RIEMANN, {2, 0, 0, 0}, NULL, NULL },
    { "P2.11a", "Problem 2.11(a)", "Geometric series in C", 2, 7, K_PROBLEM, 1,
      "\\sum_{k=0}^{n}z^k=\\frac{1-z^{n+1}}{1-z},\\qquad z\\ne1", "Induction, exactly as in $\\R$.", {0}, NS(S_P2_11a), SC_WINDING, {1, 6, 0, 0}, NULL, NULL },
    { "P2.11b", "Problem 2.11(b)", "A sum of turns (Dirichlet's kernel)", 2, 7, K_PROBLEM, 1,
      "\\sum_{k=0}^{N}e^{2\\pi ikt}=e^{i\\pi Nt}\\,\\frac{\\sin(\\pi(N+1)t)}{\\sin(\\pi t)}",
      "No trigonometric identities allowed, only turns. On the stage the unit steps bend into an arc; the chord across it is the ratio of sines.",
      {"P2.11a", "2.38", 0}, NS(S_P2_11b), SC_WINDING, {2, 7, 0, 0},
      "Erratum: (2.77) prints $\\sin(2\\pi ikt)$ and $\\cos(2\\pi ikt)$; the $i$ should not be there.",
      "The same Split as in beats (2.1): factor out the middle angle. It returns in Section 3.5 as the Dirichlet kernel." },
    { "P2.12a", "Problem 2.12(a)", "Oscillation of a real function", 2, 7, K_PROBLEM, 1,
      "\\omega(f;I)=\\sup_If-\\inf_If\\quad(f\\ \\text{real})", "Rewrite the integrability criterion using the oscillation.", {0}, NS(S_P2_12a), SC_RIEMANN, {0, 0, 0, 0},
      "Erratum: (2.78) prints $\\sup_{t\\in I}|f(x)|$, which is a sup-norm, not an oscillation. The intended definition is $\\omega(f;I)=\\sup_{x,y\\in I}|f(x)-f(y)|$, which makes (2.79) true.", NULL },
    { "P2.12b", "Problem 2.12(b)", "Integrability without lanes", 2, 7, K_PROBLEM, 1,
      "f\\ \\text{integrable (Def. 2.7)}\\iff\\Re f,\\ \\Im f\\in\\mathcal R",
      "The oscillation makes sense in $\\C$ directly, and it is squeezed between the lanes' oscillations.", {"P2.12a", 0}, NS(S_P2_12b), SC_RIEMANN, {3, 0, 0, 0}, NULL, NULL },
};
const int NPIECES = (int)(sizeof PIECES / sizeof PIECES[0]);

/* ----------------------------------------------------------- indexing */
#define MAXP 128
unsigned char PIECE_MOTIFS[MAXP][MO_COUNT];
static int g_need[MAXP][6];
static int g_ref[MAXP][12];

int piece_find(const char *id) {
    int i;
    if (!id) return -1;
    for (i = 0; i < NPIECES; i++) if (!strcmp(PIECES[i].id, id)) return i;
    return -1;
}
int piece_need(int p, int k) { return (p >= 0 && p < NPIECES && k >= 0 && k < 6) ? g_need[p][k] : -1; }
int step_ref(int p, int s) { return (p >= 0 && p < NPIECES && s >= 0 && s < 12) ? g_ref[p][s] : -1; }

int piece_users(int p, int *out, int cap) {
    int i, k, n = 0;
    for (i = 0; i < NPIECES && n < cap; i++) {
        int uses = 0;
        if (i == p) continue;
        for (k = 0; k < 6; k++) if (g_need[i][k] == p) uses = 1;
        for (k = 0; k < PIECES[i].nsteps && k < 12; k++) if (g_ref[i][k] == p) uses = 1;
        if (uses) out[n++] = i;
    }
    return n;
}

int content_init(void) {
    int i, k, bad = 0;
    for (i = 0; i < NPIECES && i < MAXP; i++) {
        const Piece *p = &PIECES[i];
        memset(PIECE_MOTIFS[i], 0, MO_COUNT);
        for (k = 0; k < 6; k++) {
            g_need[i][k] = p->needs[k] ? piece_find(p->needs[k]) : -1;
            if (p->needs[k] && g_need[i][k] < 0) bad++;
        }
        for (k = 0; k < 12; k++) g_ref[i][k] = -1;
        for (k = 0; k < p->nsteps; k++) {
            int m = p->steps[k].motif;
            if (PIECE_MOTIFS[i][m] < 255) PIECE_MOTIFS[i][m]++;
            if (k < 12) { g_ref[i][k] = piece_find(p->steps[k].ref); if (p->steps[k].ref && g_ref[i][k] < 0) bad++; }
        }
        if (p->kind == K_GROUND || p->kind == K_DEF || !p->nsteps) PIECE_MOTIFS[i][MO_GROUND] = 1;
    }
    return bad;
}
