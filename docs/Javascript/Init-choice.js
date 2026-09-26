document.addEventListener("DOMContentLoaded", () => {
  const continueButton =
    document.getElementById("sverige-init-choice");

  if (!continueButton) {
    return;
  }

  const overlay = document.createElement("div");
  overlay.id = "sverige-init-overlay";

  const systemdLogo = new URL(
    "assets/images/Systemd.png",
    document.baseURI
  ).href;

  const muslLogo = new URL(
    "assets/images/Musl.png",
    document.baseURI
  ).href;

  overlay.innerHTML = `
    <div class="sverige-init-content">

      <div class="sverige-init-title">
        Choose your path
      </div>

      <div class="sverige-init-options">

        <button
          class="sverige-init-option"
          id="sverige-systemd-choice"
          type="button"
        >
          <img
            src="${systemdLogo}"
            alt="systemd"
          >

          <div class="sverige-init-option-title">
            systemd
          </div>
        </button>

        <button
          class="sverige-init-option"
          id="sverige-busybox-choice"
          type="button"
        >
          <img
            src="${muslLogo}"
            alt="musl + BusyBox"
          >

          <div class="sverige-init-option-title">
            musl + BusyBox
          </div>
        </button>

      </div>
    </div>
  `;

  document.body.appendChild(overlay);

  continueButton.addEventListener("click", () => {
    overlay.classList.add("active");
    document.body.classList.add("sverige-choice-open");
  });

  document
    .getElementById("sverige-systemd-choice")
    .addEventListener("click", () => {
      goToGuide("systemdguide/");
    });

  document
    .getElementById("sverige-busybox-choice")
    .addEventListener("click", () => {
      goToGuide("busyboxguide/");
    });

  function goToGuide(page) {
    window.location.href = new URL(
      page,
      document.baseURI
    ).href;
  }
});
