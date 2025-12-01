target_sources(
    sereneserver
    PRIVATE rocksdb_background_error_listener.cpp rocksdb_metrics_listener.cpp
)
