# Scripts inventory

ARCH-07 기준 전체 Scripts 파일 분류. 삭제·서비스 재시작·Broker 변경·자산 저장을 승인하는 목록이 아니다. `verify.ps1`은 기존 검증을 조합한다. 각 보조 스크립트의 param과 보호 범위를 먼저 확인한다. `setup_*`/`create_*`/`prepare_*`는 대상 자산을 저장할 수 있으므로 명시된 별도 작업 범위 없이 검증을 위해 실행하지 않는다. `invoke_*archive*`의 실제 이동/삭제는 별도 승인 대상이다.

현재 사용은 해당 작업에서 사용 가능한 도구라는 뜻이며 release가 모든 스크립트를 실행한다는 뜻은 아니다. 이력 보존은 과거 migration/맵 재현용, 폐기 후보는 사용 필요를 재확인할 대상으로서 그대로 보존한다.

| 파일 | 분류 | 사용 범위/판단 |
|---|---|---|
| [README.md](README.md) | 현재 사용 | 표준 검증 진입점/보조 inventory |
| [README_rename_ma0t10_remaining_sources.md](README_rename_ma0t10_remaining_sources.md) | 이력 보존 | 기존 타입/맵 이관 재현용; 일반 검증에서 실행하지 않음 |
| [analyze_sensor_resource_lifecycle.ps1](analyze_sensor_resource_lifecycle.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [analyze_slab_repeat_stability.ps1](analyze_slab_repeat_stability.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [architecture_layers_allowlist.txt](architecture_layers_allowlist.txt) | 현재 사용 | layer gate의 고정 예외 데이터 |
| [check_project_readiness.ps1](check_project_readiness.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [create_lidar_geometry_validation_map.py](create_lidar_geometry_validation_map.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [create_lidar_semantic_material.py](create_lidar_semantic_material.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_csv_preview_performance_report.ps1](export_csv_preview_performance_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_goal_progress_blocker_report.ps1](export_goal_progress_blocker_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_judging_server_acceptance_package.ps1](export_judging_server_acceptance_package.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_judging_server_acceptance_template.ps1](export_judging_server_acceptance_template.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_large_content_cleanup_plan.ps1](export_large_content_cleanup_plan.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_large_content_decision_report.ps1](export_large_content_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_laz_compression_acceptance_package.ps1](export_laz_compression_acceptance_package.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_laz_compression_acceptance_runbook.ps1](export_laz_compression_acceptance_runbook.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_laz_compression_acceptance_template.ps1](export_laz_compression_acceptance_template.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_laz_compression_decision_report.ps1](export_laz_compression_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_laz_compression_gap_summary.ps1](export_laz_compression_gap_summary.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_laz_compressor_readiness_report.ps1](export_laz_compressor_readiness_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_local_asset_decision_evidence_template.ps1](export_local_asset_decision_evidence_template.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_local_asset_decision_report.ps1](export_local_asset_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_acceptance_package.ps1](export_monitor_wbp_acceptance_package.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_acceptance_runbook.ps1](export_monitor_wbp_acceptance_runbook.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_acceptance_template.ps1](export_monitor_wbp_acceptance_template.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_decision_report.ps1](export_monitor_wbp_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_evidence_todo.ps1](export_monitor_wbp_evidence_todo.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_gap_summary.ps1](export_monitor_wbp_gap_summary.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_post_edit_hash_report.ps1](export_monitor_wbp_post_edit_hash_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_monitor_wbp_preflight_report.ps1](export_monitor_wbp_preflight_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_payload_contract_report.ps1](export_payload_contract_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_point_cloud_renderer_acceptance_package.ps1](export_point_cloud_renderer_acceptance_package.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_point_cloud_renderer_decision_report.ps1](export_point_cloud_renderer_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_real_sensor_adapter_deployment_package.ps1](export_real_sensor_adapter_deployment_package.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_real_sensor_adapter_deployment_template.ps1](export_real_sensor_adapter_deployment_template.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_real_sensor_adapter_gap_summary.ps1](export_real_sensor_adapter_gap_summary.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_real_sensor_adapter_readiness_report.ps1](export_real_sensor_adapter_readiness_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_runtime_config_acceptance_template.ps1](export_runtime_config_acceptance_template.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_runtime_config_decision_report.ps1](export_runtime_config_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_sample_content_decision_report.ps1](export_sample_content_decision_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_unused_content_archive_evidence.ps1](export_unused_content_archive_evidence.ps1) | 폐기 후보 | 이전 archive workflow; 현재 승인 범위에 포함되지 않음, 삭제 금지 |
| [export_websocket_broker_smoke_report.ps1](export_websocket_broker_smoke_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [export_websocket_transaction_registration_report.ps1](export_websocket_transaction_registration_report.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [inspect_binary_pcd.py](inspect_binary_pcd.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [inspect_launcher_package_map.py](inspect_launcher_package_map.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [invoke_local_decision_precommit_gate.ps1](invoke_local_decision_precommit_gate.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [invoke_unused_content_archive.ps1](invoke_unused_content_archive.ps1) | 폐기 후보 | 이전 archive workflow; 현재 승인 범위에 포함되지 않음, 삭제 금지 |
| [prepare_monitor_wbp_editor_review.ps1](prepare_monitor_wbp_editor_review.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [prepare_slab_analysis_pie.py](prepare_slab_analysis_pie.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [prepare_slab_visibility_pie.py](prepare_slab_visibility_pie.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [read_pcd_context.py](read_pcd_context.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [rename_ma0t10_remaining_sources.ps1](rename_ma0t10_remaining_sources.ps1) | 이력 보존 | 기존 타입/맵 이관 재현용; 일반 검증에서 실행하지 않음 |
| [report_local_project_status.ps1](report_local_project_status.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [report_precommit_summary.ps1](report_precommit_summary.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_artemis_stream_smoke.ps1](run_artemis_stream_smoke.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_committed_sensor_map_smoke.ps1](run_committed_sensor_map_smoke.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_csv_preview_performance_evidence.ps1](run_csv_preview_performance_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_dtcore_final_regression.ps1](run_dtcore_final_regression.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_final_build_matrix.ps1](run_final_build_matrix.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_final_sensor_endurance.ps1](run_final_sensor_endurance.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_final_slab_output_test.ps1](run_final_slab_output_test.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_fullspec_performance_evidence.ps1](run_fullspec_performance_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_lidar_geometry_stream_test.ps1](run_lidar_geometry_stream_test.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_pcd_demand_acceptance.ps1](run_pcd_demand_acceptance.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_point_cloud_rhi_smoke.ps1](run_point_cloud_rhi_smoke.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_scenario_data_reliability_faults.ps1](run_scenario_data_reliability_faults.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_sensor_map_stream_rhi_smoke.ps1](run_sensor_map_stream_rhi_smoke.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_slab_repeat_stability.ps1](run_slab_repeat_stability.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_slab_scenario_replay_smoke.ps1](run_slab_scenario_replay_smoke.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_smoke_tests.ps1](run_smoke_tests.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [run_websocket_lidar_smoke_evidence.ps1](run_websocket_lidar_smoke_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [setup_lidar_niagara_assets.ps1](setup_lidar_niagara_assets.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [setup_sensor_placement_widget.py](setup_sensor_placement_widget.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [setup_sensor_refactor_test_map.py](setup_sensor_refactor_test_map.py) | 이력 보존 | 기존 타입/맵 이관 재현용; 일반 검증에서 실행하지 않음 |
| [setup_sensor_scale_stress_map.py](setup_sensor_scale_stress_map.py) | 이력 보존 | 기존 타입/맵 이관 재현용; 일반 검증에서 실행하지 않음 |
| [setup_sensor_test_map.py](setup_sensor_test_map.py) | 이력 보존 | 기존 타입/맵 이관 재현용; 일반 검증에서 실행하지 않음 |
| [setup_slab_analysis_materials.py](setup_slab_analysis_materials.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [setup_slab_readability_assets.py](setup_slab_readability_assets.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [setup_slab_replay_widget.py](setup_slab_replay_widget.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [setup_slab_scenario_validation.py](setup_slab_scenario_validation.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [update_monitor_wbp_acceptance_hash_evidence.ps1](update_monitor_wbp_acceptance_hash_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [update_monitor_wbp_manual_acceptance_sections.ps1](update_monitor_wbp_manual_acceptance_sections.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [update_monitor_wbp_manual_evidence_paths.ps1](update_monitor_wbp_manual_evidence_paths.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_architecture_layers.ps1](validate_architecture_layers.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_dtcore_integration_assets.py](validate_dtcore_integration_assets.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_dtcore_submodule_guard.ps1](validate_dtcore_submodule_guard.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_large_content_decision_policy.ps1](validate_large_content_decision_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_laz_compression_acceptance_evidence.ps1](validate_laz_compression_acceptance_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_laz_placeholder_policy.ps1](validate_laz_placeholder_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_lidar_widget_assets.py](validate_lidar_widget_assets.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_local_asset_decision_evidence_workflow.ps1](validate_local_asset_decision_evidence_workflow.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_local_decision_precommit_gate_policy.ps1](validate_local_decision_precommit_gate_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_monitor_wbp_acceptance_evidence.ps1](validate_monitor_wbp_acceptance_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_monitor_widget_policy.ps1](validate_monitor_widget_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_opt_in_inventory.ps1](validate_opt_in_inventory.ps1) | 현재 사용 | 표준 검증 진입점/보조 inventory |
| [validate_payload_contract.ps1](validate_payload_contract.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_payload_fixtures.ps1](validate_payload_fixtures.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_payload_schema_review_policy.ps1](validate_payload_schema_review_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_point_cloud_preview_policy.ps1](validate_point_cloud_preview_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_real_sensor_adapter_deployment_evidence.ps1](validate_real_sensor_adapter_deployment_evidence.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_real_sensor_adapter_plan.ps1](validate_real_sensor_adapter_plan.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_runtime_config_policy.ps1](validate_runtime_config_policy.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_sensor_placement_ab.ps1](validate_sensor_placement_ab.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_sensor_resource_lifecycle.ps1](validate_sensor_resource_lifecycle.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_sensor_tool_widgets.py](validate_sensor_tool_widgets.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_sensor_v2_refactor.ps1](validate_sensor_v2_refactor.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_server_transport_contract.ps1](validate_server_transport_contract.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_slab_assets.py](validate_slab_assets.py) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [validate_websocket_lidar_live_sample.ps1](validate_websocket_lidar_live_sample.ps1) | 현재 사용 | 명시적인 관련 fixture/검수/보고 작업에 사용; 매 verify 실행 대상은 아님 |
| [verify.ps1](verify.ps1) | 현재 사용 | 표준 검증 진입점/보조 inventory |
