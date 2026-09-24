(() => {
    const PARALLAX_SPEED = 0.25;

    let layer = null;
    let ticking = false;

    function findLayer() {
        layer = document.querySelector(".sverige-parallax");
    }

    function updateParallax() {
        ticking = false;

        if (!layer) {
            findLayer();
        }

        if (!layer) {
            return;
        }

        const reducedMotion = window.matchMedia(
            "(prefers-reduced-motion: reduce)"
        ).matches;

        if (reducedMotion) {
            layer.style.transform = "translate3d(0, 0, 0)";
            return;
        }

        const scrollY = window.scrollY;

        const offset = -(scrollY * PARALLAX_SPEED);

        layer.style.transform =
            `translate3d(0, ${offset}px, 0)`;
    }

    function requestParallaxUpdate() {
        if (!ticking) {
            window.requestAnimationFrame(updateParallax);
            ticking = true;
        }
    }

    function initParallax() {
        findLayer();
        updateParallax();
    }

    window.addEventListener(
        "scroll",
        requestParallaxUpdate,
        { passive: true }
    );

    window.addEventListener(
        "resize",
        requestParallaxUpdate,
        { passive: true }
    );

    document.addEventListener(
        "DOMContentLoaded",
        initParallax
    );

    if (typeof document$ !== "undefined") {
        document$.subscribe(() => {
            initParallax();
        });
    }
})();
