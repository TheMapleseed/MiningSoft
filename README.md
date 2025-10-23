# MiningSoft - Monero Miner for Apple Silicon

A high-performance, enterprise-grade Monero (XMR) miner optimized for all Apple Silicon processors (M1, M2, M3, M4, M5). Built with **Clang only** - no external dependencies required. Features comprehensive testing, error handling, and monitoring systems.

## 🚀 Key Features

- **✅ Clang-Only Build** - No CMake, no external dependencies, just Clang
- **🧪 Comprehensive Testing** - 15+ startup tests run on every launch
- **🔧 Enterprise Error Handling** - Production-ready error recovery and monitoring
- **⚡ Apple Silicon Optimized** - ARM64 NEON intrinsics and hardware acceleration
- **🌐 Multi-Pool Support** - Failover, priority switching, and load balancing
- **💾 Advanced Memory Management** - Hardware-accelerated memory with dynamic scaling
- **📊 Real-Time Monitoring** - Performance dashboard and statistics
- **🔒 Enhanced Security** - Wallet validation and secure configuration
- **🖥️ Interactive CLI** - Full-featured command-line interface with wallet management
- **🔄 Auto-Recovery** - Automatic error recovery and system healing
- **📱 macOS Integration** - LaunchDaemon support for boot startup

## 📋 Requirements

- **macOS 11.0+** (Big Sur or later)
- **Apple Silicon** (M1, M2, M3, M4, or M5)
- **Clang Compiler** (included with Xcode Command Line Tools)
- **No external dependencies** - completely self-contained

## 🛠️ Building with Clang

MiningSoft is designed to be built using **only Clang**, following the same approach as the official Monero project. No CMake, Meson, or other build systems required.

### Prerequisites

1. **Install Xcode Command Line Tools** (includes Clang):
   ```bash
   xcode-select --install
   ```

2. **Verify Clang Installation**:
   ```bash
   clang++ --version
   ```

### Quick Build

```bash
# Clone the repository
git clone https://github.com/TheMapleseed/MiningSoft.git
cd MiningSoft

# Build with Clang (single command)
make

# Run the miner
./monero-miner --cli
```

### Advanced Build Options

```bash
# Build with optimizations
make clean && make

# Build and run tests
make test

# Build test runner
make test-runner

# Clean build artifacts
make clean
```

### Manual Clang Build (Alternative)

If you prefer to build manually with Clang:

```bash
# Compile all sources with Clang
clang++ -std=c++23 -O3 -flto -fvectorize \
        -DAPPLE_SILICON_OPTIMIZED -DAPPLE_SILICON_UNIVERSAL \
        -mfloat-abi=hard -mfpu=neon \
        -Iinclude -Isrc \
        -framework Foundation -framework IOKit -framework Accelerate \
        src/*.cpp -o monero-miner
```

## 🧪 Startup Testing System

MiningSoft runs **comprehensive startup tests** on every launch to ensure system integrity:

### Test Categories
- **🖥️ System**: Apple Silicon detection, memory requirements, OS compatibility
- **⚙️ Config**: Configuration loading and validation
- **🌐 Network**: DNS resolution and connectivity tests
- **⛏️ Mining**: RandomX algorithm and mining components
- **💾 Memory**: Memory management and allocation
- **📊 Performance**: Performance monitoring systems
- **🔒 Security**: Wallet validation and security checks
- **🔗 Integration**: Full system integration tests

### Test Results Display
```
╔══════════════════════════════════════════════════════════════════════════════╗
║                           STARTUP TEST RESULTS                               ║
╚══════════════════════════════════════════════════════════════════════════════╝

✅ System Requirements                    System        15ms
✅ Configuration Loading                  Config        8ms
✅ Mining Components                      Mining        45ms
✅ RandomX Algorithm                      Mining        23ms
✅ Memory Management                      Memory        12ms
✅ Performance Monitoring                 Performance   7ms
✅ Security Validation                    Security      3ms
✅ System Integration                     Integration   156ms

Total Tests: 16    Passed: 16 ✅    Failed: 0 ❌
Overall Status: ALL TESTS PASSED
```

## ⚙️ Configuration

### Basic Configuration

Create a `config.json` file:

```json
{
  "pool.url": "stratum+tcp://pool.supportxmr.com:3333",
  "pool.username": "your_wallet_address",
  "pool.password": "x",
  "pool.protocol": "stratum_v1",
  "mining.threads": 0,
  "mining.intensity": 100,
  "logging.level": "info",
  "logging.console": true,
  "startup.autoStart": false,
  "startup.runTests": true,
  "startup.displayResults": true
}
```

### Multi-Pool Configuration

```json
{
  "pools.hashvault.name": "HashVault",
  "pools.hashvault.url": "stratum+tcp://pool.monero.hashvault.pro:4444",
  "pools.hashvault.username": "your_wallet_address",
  "pools.hashvault.password": "x",
  "pools.hashvault.priority": 8,
  "pools.hashvault.enabled": true,
  "multipool.failoverEnabled": true,
  "multipool.autoSwitchEnabled": true
}
```

## 🚀 Usage

### Command Line Interface

```bash
# Start with CLI (recommended)
./monero-miner --cli

# Start mining directly
./monero-miner --mining

# Emergency bypass (skip tests)
./monero-miner --emergency-bypass --cli

# Show help
./monero-miner --help
```

### CLI Commands

Once in the CLI, you can use these commands:

```
miningsoft> help
Available commands:
  start          - Start mining
  stop           - Stop mining
  status         - Show mining status
  stats          - Show performance statistics
  pools          - Manage mining pools
  wallet         - Manage wallet addresses
  config         - Manage configuration
  test           - Run system tests
  monitor        - Show real-time monitoring
  help           - Show this help
  exit           - Exit the application

miningsoft> wallet add
Enter wallet address: 9wviCeWe2D8XS82k2ovp5EUYLzBt9pYNW2LXUFsZiv8S3Mt21FZ5qQaAroko1enzw3eGr9qC7X1D7Geoo2RrAotYPwq9Gm8
Wallet added successfully!

miningsoft> start
Mining started successfully!
Hash rate: 2.5 KH/s
```

### macOS LaunchDaemon Integration

For automatic startup on boot:

```bash
# Install as system daemon
sudo ./install_daemon.sh

# Manage the daemon
./manage_miner.sh start    # Start mining
./manage_miner.sh stop     # Stop mining
./manage_miner.sh status   # Check status
./manage_miner.sh logs     # View logs

# Uninstall daemon
sudo ./uninstall_daemon.sh
```

## 🧪 Testing and Validation

### Run Comprehensive Tests

```bash
# Run all tests
./run_tests.sh

# Test startup system
./test_startup.sh

# Monitor errors in real-time
./monitor_errors.sh
```

### Test Categories

- **Unit Tests**: Individual component testing
- **Integration Tests**: Full system integration
- **Performance Tests**: Benchmarking and optimization
- **Error Handling Tests**: Error recovery validation
- **Security Tests**: Wallet and configuration validation

## 📊 Performance Monitoring

### Real-Time Dashboard

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                        MININGSOFT PERFORMANCE DASHBOARD                     ║
╚══════════════════════════════════════════════════════════════════════════════╝

⛏️  Mining Status:     ACTIVE
📊 Hash Rate:          2.5 KH/s
🎯 Shares Submitted:   1,234
✅ Shares Accepted:    1,200 (97.2%)
❌ Shares Rejected:    34 (2.8%)
🌡️  CPU Temperature:   65°C
💾 Memory Usage:       512 MB
⏱️  Uptime:            2h 15m 30s

🔗 Active Pool:        pool.supportxmr.com:3333
👤 Wallet:             9wviCe...Gm8
⚡ Efficiency:         94.5%
```

### Performance Metrics

- **Hash Rate**: Real-time and average hashing performance
- **Share Statistics**: Submission, acceptance, and rejection rates
- **System Resources**: CPU, memory, and thermal monitoring
- **Pool Performance**: Connection status and latency
- **Efficiency Scoring**: Overall mining efficiency

## 🔧 Error Handling and Recovery

### Automatic Error Recovery

- **Network Errors**: Automatic reconnection and pool switching
- **Memory Errors**: Dynamic memory management and cleanup
- **Thermal Errors**: Automatic throttling and cooling
- **Configuration Errors**: Validation and correction suggestions

### Error Monitoring

```bash
# Monitor errors in real-time
./monitor_errors.sh

# View error statistics
./monitor_errors.sh stats

# Analyze error patterns
./monitor_errors.sh analyze
```

## 🛡️ Security Features

### Wallet Management

- **Address Validation**: Monero mainnet and testnet address validation
- **Secure Storage**: Encrypted wallet configuration
- **Multiple Wallets**: Support for multiple wallet addresses
- **Import/Export**: Wallet configuration backup and restore

### Security Validation

- **Input Sanitization**: All inputs validated and sanitized
- **Configuration Security**: Secure default settings
- **Error Handling**: Secure error reporting without information leakage
- **Memory Protection**: Secure memory management and cleanup

## 🏗️ Architecture

### Core Components

- **Miner Core**: Main mining engine with RandomX implementation
- **Pool Manager**: Multi-pool support with failover and load balancing
- **Memory Manager**: Hardware-accelerated memory management
- **Performance Monitor**: Real-time statistics and monitoring
- **Error Handler**: Comprehensive error handling and recovery
- **CLI Manager**: Interactive command-line interface
- **Test Framework**: Comprehensive testing and validation system

### Build System

- **Clang Compiler**: Modern C++23 with ARM64 optimizations
- **Apple Frameworks**: Foundation, IOKit, Accelerate
- **No External Dependencies**: Completely self-contained
- **Cross-Platform Ready**: Can be adapted for other ARM64 platforms

## 📈 Performance Optimization

### Apple Silicon Optimizations

- **ARM64 NEON Intrinsics**: Vectorized operations for maximum performance
- **Cache Optimization**: Optimized for Apple Silicon cache hierarchy
- **Memory Management**: Efficient allocation with hardware acceleration
- **Thermal Awareness**: Dynamic performance scaling based on temperature

### Recommended Settings

| Processor | Cores | GPU | Intensity | Expected Hash Rate |
|-----------|-------|-----|-----------|-------------------|
| M1        | 8     | ✅  | 100       | 2.0-2.5 KH/s      |
| M1 Pro    | 8-10  | ✅  | 100       | 2.5-3.0 KH/s      |
| M1 Max    | 10-12 | ✅  | 100       | 3.0-3.5 KH/s      |
| M2        | 8     | ✅  | 100       | 2.2-2.7 KH/s      |
| M2 Pro    | 10-12 | ✅  | 100       | 2.8-3.3 KH/s      |
| M2 Max    | 12    | ✅  | 100       | 3.2-3.8 KH/s      |
| M3        | 8     | ✅  | 100       | 2.5-3.0 KH/s      |
| M3 Pro    | 10-12 | ✅  | 100       | 3.0-3.5 KH/s      |
| M3 Max    | 12-14 | ✅  | 100       | 3.5-4.0 KH/s      |

## 🔍 Troubleshooting

### Common Issues

1. **Build Errors**: Ensure Xcode Command Line Tools are installed
2. **Test Failures**: Check system requirements and configuration
3. **Permission Errors**: Run with appropriate permissions
4. **Thermal Throttling**: Check cooling and reduce intensity
5. **Low Hashrate**: Verify pool connection and configuration

### Debug Mode

```bash
# Enable debug logging
./monero-miner --cli --log-level debug

# Run with emergency bypass
./monero-miner --emergency-bypass --cli

# Test specific components
./test_startup.sh config
./test_startup.sh errors
```

## 📚 Documentation

### Additional Resources

- **Configuration Guide**: Detailed configuration options
- **API Reference**: Internal API documentation
- **Performance Tuning**: Optimization guidelines
- **Error Codes**: Complete error code reference
- **Troubleshooting**: Common issues and solutions

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new features
5. Run the test suite: `./run_tests.sh`
6. Submit a pull request

### Development Setup

```bash
# Clone and build
git clone https://github.com/TheMapleseed/MiningSoft.git
cd MiningSoft
make

# Run tests
make test
./run_tests.sh

# Run startup tests
./test_startup.sh
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ⚠️ Disclaimer

- Mining cryptocurrency may not be profitable
- Consider electricity costs and hardware wear
- Mining may void device warranties
- Use at your own risk
- Always verify wallet addresses before mining

## 🆘 Support

For support and questions:

- **GitHub Issues**: Create an issue for bugs or feature requests
- **Documentation**: Check the troubleshooting section
- **Configuration**: Review the configuration options
- **Testing**: Run the test suite to diagnose issues

## 🎯 Why MiningSoft?

- **✅ Clang-Only Build** - No complex build systems or dependencies
- **🧪 Comprehensive Testing** - Every launch validates system integrity
- **🔧 Enterprise Error Handling** - Production-ready error recovery
- **⚡ Apple Silicon Optimized** - Maximum performance on Apple hardware
- **🌐 Multi-Pool Support** - Redundancy and failover capabilities
- **💾 Advanced Memory Management** - Hardware-accelerated memory handling
- **📊 Real-Time Monitoring** - Complete visibility into mining operations
- **🔒 Enhanced Security** - Secure wallet management and validation
- **🖥️ Interactive CLI** - Full-featured command-line interface
- **🔄 Auto-Recovery** - Automatic error recovery and system healing

**MiningSoft** - The most advanced, reliable, and user-friendly Monero miner for Apple Silicon! 🚀