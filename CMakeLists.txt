include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(scratch_crypto_sim
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ascon_crypto.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto_app.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/event_emitter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memostp_protocol.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/metrics_collector.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/node_monitor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/snake_optimizer.cc
)

target_link_libraries(scratch_crypto_sim
    ns3::core
    ns3::network
    ns3::internet
    ns3::applications

    ns3::mobility
    ns3::wifi
    ns3::energy
    ns3::olsr
    ns3::flow-monitor
)
