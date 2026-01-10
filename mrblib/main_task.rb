if RUBY_ENGINE == "mruby/c"
  require "numeric-ext"
end
require "machine"
require "watchdog"
Watchdog.disable
require "shell"
require "irq"
STDOUT = IO.new
STDIN = IO.new

Machine.set_hwclock(0)

begin
  sleep 1
  STDIN.echo = false
  puts "Initializing FLASH disk as the root volume... "
  Shell.setup_root_volume(:flash, label: "R2P2")
  Shell.setup_system_files

  # Load HID config from /config.txt
  CONFIG_FILE = "/config.txt"
  if File.exist?(CONFIG_FILE)
    puts "Loading HID config from #{CONFIG_FILE}..."
    flags = 0
    File.open(CONFIG_FILE, "r") do |f|
      f.each_line do |line|
        parts = line.strip.split(":")
        next if parts.size < 2
        key = parts[0].strip
        value = parts[1].strip.downcase
        enabled = (value == "true")
        case key
        when "usb_keyboard_report"
          flags |= 0x01 if enabled
        when "usb_consumer_report"
          flags |= 0x02 if enabled
        when "usb_mouse_report"
          flags |= 0x04 if enabled
        end
      end
    end
    # RawHID is always enabled
    flags |= 0x08
    if USB.save_hid_config(flags)
      puts "HID config saved (flags=0x#{flags.to_s(16)}). Reboot to apply."
    else
      puts "HID config unchanged (flags=0x#{flags.to_s(16)})."
    end
  else
    # Create default config.txt
    puts "Creating default #{CONFIG_FILE}..."
    File.open(CONFIG_FILE, "w") do |f|
      f.puts "usb_keyboard_report: true"
      f.puts "usb_consumer_report: true"
      f.puts "usb_mouse_report: true"
    end
    # Save default config to Flash (all enabled = 0x0F)
    USB.save_hid_config(0x0F)
    puts "Default HID config created."
  end

  Shell.bootstrap("/etc/init.d/r2p2")

  shell = Shell.new(clean: true)
  puts "Starting shell...\n\n"

  shell.show_logo
  shell.start
rescue => e
  puts "#{e.message} (#{e.class})"
end

