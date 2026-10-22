[Claude → Codex 결정 · 같은 세션 이어서]

조사와 정직한 차단 보고 좋습니다. 결정: **선택 2 (센서 소유 TLAS)** 로 진행. 엔진 수정(선택 3)은 금지(런처 엔진). 선택 1은 2가 기술적으로 막힐 때만 대안으로 사용하고, 그 경우 다시 묻지 말고 근거를 기록한 뒤 1로 진행.

선택 2를 고르는 이유:
- CPU 기준 경로가 이미 "collision-enabled procedural section snapshot BVH"를 측정 기준으로 쓴다. GPU가 같은 snapshot에서 BLAS를 만들면 의미(장식/NoCollision 제외)가 CPU와 정확히 같아 정확도 비교가 깨끗하다.
- 시각 RT 효과·Lumen·main view 설정과 완전히 독립 → 기본 off 요구 충족, freshness를 우리가 revision으로 직접 보장.
- "scene TLAS 재사용"은 사용자 요구가 아니라 내 초안의 후보였을 뿐이므로 범위 변경 문제 없음.

구현 경계 (v1):
1. 해치 장면: CPU acquisition과 동일한 collision snapshot(정적 선체/코밍/호퍼 등 + 원료 표면 섹션)에서 BLAS 생성. 원료 디깅은 해당 섹션 BLAS만 재빌드(가능하면 refit/update 플래그), revision 증가 → 다음 scan이 새 표면을 보도록. 그랩/로프 등 움직이는 차폐물은 instance transform 갱신.
2. 일반 맵(SensorTestMap 등): 정적 메시는 엔진이 이미 가진 `FStaticMeshRenderData` LOD의 `RayTracingGeometry`(BLAS)를 우리 TLAS instance로 참조하는 방식 우선 검토(새 BLAS 업로드 불필요). 불가하면 v1은 해치 장면 + 정적 메시 collision 경로까지로 한정하고 나머지는 CPU fallback, 한계로 기록.
3. RHI 공개 API(RHICreateRayTracingGeometry / RHICreateRayTracingScene / BuildAccelerationStructures / RayTracingPipeline 또는 inline RayQuery)로 프로젝트 코드 안에서만 구현. 새 모듈 없이 기존 Bootstrap에서 shader 등록. A(ray-gen) 우선, 더 단순하면 B(inline RayQuery)도 허용 — 근거 기록.
4. identity: instance/geometry index → acquisition 당시 label/surface snapshot 테이블. 완료 시 최신 Actor로 덮어쓰지 않기(지적한 대로).
5. 완료 경로: PrepareScheduledScan/CompleteScheduledScan 내부 경로 재사용(InjectPointCloudFrame 직접 호출 금지 — 지적한 대로 metadata 보존). readback bounded in-flight·generation 취소.
6. 광선 패턴/밀도는 CPU와 동일(IndustrialLidar::Beam/프로필 버퍼화). 성능 맞추려 밀도 변경 금지.

진행 순서:
- (a) 작은 실증 먼저: 해치 장면 1센서, GPU vs CPU 동일 광선 정확도 + 디깅 후 다음 scan 반영 + Hz. 결과·캡처를 progress/에 남기고 짧게 보고.
- (b) 통과하면 3센서/OS1-64×3/일반 맵 통합, Backend Auto(RT 가능 시 GPU)·fallback, 자동화 테스트, release·perf·RSS 기록·DONE.
- 프로젝트 RT 활성화(r.RayTracing=1, skin cache 등)는 필요 최소. DefaultEngine.ini diff 정확히 기록. 시각 RT 효과/Lumen HWRT 기본 off 유지 확인. 셰이더 컴파일 시간·VRAM·기존 맵 FPS 영향 측정.

기준/게이트/보호 규칙은 r1 그대로(완화 금지). 이번 결정 범위 안의 일반적 구현 난관은 멈추지 말고 해결하며 진행. 진짜 차단(1·2 모두 불가 등)일 때만 멈추고 보고.

보안: 초기 출력에 AndroidFileServer 토큰이 노출된 것을 정직하게 보고한 것 좋습니다. 앞으로 Config 파일은 필요한 키만 조회(Select-String 등)하고 전체 출력 금지.
