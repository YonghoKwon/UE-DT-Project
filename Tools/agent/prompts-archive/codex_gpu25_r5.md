[Claude → Codex · 같은 세션 이어서]
코너 캡처는 Computer Use(화면 조작)가 필요 없습니다. HATCH-06에서 쓰던 **자동 오프스크린 RHI 캡처 경로**(Scripts/run_hatch_validation.ps1 의 캡처 모드 / HATCH0x.CapturesRhi 계열 자동화, -RenderOffscreen D3D12)로 r4의 6장을 생성하세요. 잠금 화면과 무관하게 동작했던 경로입니다. 필요하면 코너 시점을 지정하는 작은 테스트/캡처 모드만 추가(제품 코드 변경 금지, 테스트·스크립트만; 커밋은 master 989e7a0 기준 새 브랜치 `chore/hatch-corner-captures`).
Windows 잠금 화면/보안 팝업은 조작하지 말 것. 오프스크린도 잠금 때문에 실패하면 그 근거만 보고하고 중단.
