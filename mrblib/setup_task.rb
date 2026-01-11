if RUBY_ENGINE == "mruby/c"
  require "numeric-ext"
end
require "machine"
require "shell"
require 'logger'
require 'watchdog'
Watchdog.disable

STDOUT = IO.new
STDIN = IO.new

# ファイルシステム初期化
Shell.setup_root_volume(:flash, label: "R2P2")
Shell.setup_system_files
if VFS.exist?("setup_task.log")
  VFS.unlink("setup_task.log")
end
logger = Logger.new("setup_task.log", level: :debug)

# 設定ファイルの読み込みと実行
CONFIG_FILE = "/home/config.rb"

if File.exist?(CONFIG_FILE)
  logger.debug "setup_task: Loading #{CONFIG_FILE}..."
  begin
    load CONFIG_FILE
    logger.debug "setup_task: Config loaded successfully"
  rescue => e
    logger.debug "setup_task: Failed - #{e.message}"
  end
else
  logger.debug "setup_task: #{CONFIG_FILE} not found"
end

logger.debug "setup_task: Complete"

logger.close
