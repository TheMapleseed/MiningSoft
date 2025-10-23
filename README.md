# Monero Miner for Apple Silicon

A high-performance Monero (XMR) miner optimized for all Apple Silicon processors (M1, M2, M3, M4, M5). This miner leverages ARM64 optimizations, Metal GPU acceleration, Vector Processor support, and intelligent CPU demand throttling to provide efficient mining on Apple devices.

## Features

- **Universal Apple Silicon Support**: Compatible with M1, M2, M3, M4, and M5 processors
- **Metal GPU Support**: Utilizes Apple's Metal API for GPU acceleration
- **Vector Processor Support**: Leverages M5's advanced Vector Processors for maximum performance
- **Intelligent CPU Throttling**: Automatically adjusts mining based on your CPU usage
- **RandomX Algorithm**: Full RandomX implementation optimized for Apple Silicon
- **Mining Pool Support**: Compatible with standard Monero mining pools
- **Performance Monitoring**: Comprehensive metrics and optimization recommendations
- **Enterprise Grade**: Secure, reliable, and production-ready code
- **C23 Compliant**: Fully compliant with C23 (ISO/IEC 9899:2024) standards

## Requirements

- macOS 11.0 or later
- Apple Silicon processor (M1, M2, M3, M4, or M5)
- Xcode Command Line Tools
- CMake 3.20 or later

## Installation

### Prerequisites

1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```

2. Install CMake (if not already installed):
   ```bash
   brew install cmake
   ```

### Building

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/MiningSoft.git
   cd MiningSoft
   ```

2. Build the miner:
   ```bash
   chmod +x scripts/build.sh
   ./scripts/build.sh
   ```

3. Test C23 compliance (optional):
   ```bash
   chmod +x scripts/test_c23_compliance.sh
   ./scripts/test_c23_compliance.sh
   ```

4. The executable will be created in the `build/` directory as `monero-miner`.

## Configuration

### Configuration File

Create a `config.json` file in the miner directory:

```json
{
  "mining": {
    "algorithm": "randomx",
    "threads": 0,
    "useGPU": true,
    "useHugePages": false,
    "intensity": 100
  },
  "pool": {
    "url": "stratum+tcp://pool.monero.hashvault.pro:4444",
    "username": "your_wallet_address",
    "password": "x",
    "workerId": "apple-silicon-miner"
  },
  "thermal": {
    "maxCpuTemp": 85.0,
    "maxGpuTemp": 90.0,
    "maxSystemTemp": 80.0,
    "enableThrottling": true
  },
  "logging": {
    "level": "info",
    "file": "miner.log",
    "console": true
  }
}
```

### Command Line Options

```bash
./monero-miner [options]

Options:
  -c, --config <file>    Configuration file (default: config.json)
  -p, --pool <url>       Mining pool URL
  -u, --user <username>  Pool username
  -w, --pass <password>  Pool password
  -t, --threads <num>    Number of threads (0 = auto)
  --gpu                  Enable GPU mining
  --no-gpu               Disable GPU mining
  --intensity <0-100>    Mining intensity (default: 100)
  --thermal-limit <temp> CPU thermal limit in Celsius (default: 85)
  --log-level <level>    Log level: debug, info, warn, error (default: info)
  --log-file <file>      Log file path (default: console only)
  --help                 Show help message
  --version              Show version information
```

## Usage Examples

### Basic Usage

```bash
# Using configuration file
./monero-miner -c config.json

# Using command line arguments
./monero-miner -p stratum+tcp://pool.monero.hashvault.pro:4444 -u your_wallet_address -w x
```

### Advanced Usage

```bash
# Custom thread count and thermal limits
./monero-miner -p stratum+tcp://pool.supportxmr.com:443 -u your_wallet_address -w x -t 8 --thermal-limit 80

# GPU mining with custom intensity
./monero-miner -c config.json --gpu --intensity 80

# Debug logging
./monero-miner -c config.json --log-level debug --log-file debug.log
```

## Performance Optimization

### Apple Silicon Specific Optimizations

- **ARM64 Vectorization**: Uses NEON intrinsics for vectorized operations
- **Cache Optimization**: Optimized for Apple Silicon cache hierarchy
- **Memory Management**: Efficient memory allocation and management
- **Thermal Awareness**: Dynamic performance scaling based on temperature

### Recommended Settings

- **M1**: Use all 8 cores, enable GPU mining
- **M1 Pro/Max**: Use 8-10 cores, enable GPU mining
- **M2**: Use all 8 cores, enable GPU mining
- **M2 Pro/Max**: Use 10-12 cores, enable GPU mining
- **M3**: Use all 8 cores, enable GPU mining

## Thermal Management

The miner includes advanced thermal management to prevent overheating:

- **Real-time Monitoring**: Continuous temperature monitoring
- **Automatic Throttling**: Reduces performance when temperatures are high
- **Emergency Shutdown**: Automatic shutdown in thermal emergencies
- **Configurable Limits**: Customizable temperature thresholds

## Monitoring and Logging

### Performance Metrics

- Current and average hashrate
- Total hashes computed
- Share acceptance rate
- System resource usage
- Temperature monitoring
- Efficiency scoring

### Logging

- Multiple log levels (debug, info, warn, error)
- Console and file output
- Automatic log rotation
- Colored console output

## Security Features

- **Enterprise Grade**: Secure, production-ready code
- **No Backdoors**: Open source and auditable
- **Safe Defaults**: Conservative security settings
- **Input Validation**: Comprehensive input validation and sanitization

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure Xcode Command Line Tools are installed
2. **Permission Errors**: Run with appropriate permissions
3. **Thermal Throttling**: Check cooling and reduce intensity
4. **Low Hashrate**: Verify pool connection and configuration

### Debug Mode

Enable debug logging for detailed information:

```bash
./monero-miner -c config.json --log-level debug
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Disclaimer

- Mining cryptocurrency may not be profitable
- Consider electricity costs and hardware wear
- Mining may void device warranties
- Use at your own risk

## Support

For support and questions:
- Create an issue on GitHub
- Check the troubleshooting section
- Review the configuration options

## C23 Compliance

This miner is fully compliant with C23 (ISO/IEC 9899:2024) standards, featuring:

- **Modern C23 Features**: Uses latest C23 language features including `auto` keyword, `constexpr`, and improved type inference
- **Standard Compliance**: Follows C23 standard library specifications and best practices
- **Future-Proof**: Built with the latest C standard for maximum compatibility and performance
- **Compliance Testing**: Includes automated C23 compliance verification

### C23 Features Used

- `auto` keyword for type inference
- `constexpr` for compile-time constants
- Modern memory management with `aligned_alloc`
- Enhanced atomic operations
- Improved string handling
- Modern threading support

## Changelog

### Version 1.0.0
- Initial release
- Apple Silicon optimization
- Metal GPU support
- Thermal management
- RandomX algorithm implementation
- Mining pool connectivity
- Performance monitoring
- Comprehensive logging
- C23 compliance (ISO/IEC 9899:2024)
