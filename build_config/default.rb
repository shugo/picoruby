#
# This build config works also in mruby/build_config
#

MRuby::Build.new do |conf|
  conf.toolchain

  disable_presym
  conf.mrbcfile = "#{conf.build_dir}/bin/picorbc"

  ENV['MRUBYC_BRANCH'] = "for_prk_firmware_0.9.18"
  conf.gem github: 'shugo/mruby-mrubyc', branch: 'for_prk_firmware_0.9.18'
  conf.gem github: 'shugo/mruby-pico-compiler', branch: 'for_prk_firmware_0.9.18'
  conf.gem github: 'shugo/mruby-bin-picorbc', branch: 'for_prk_firmware_0.9.18'
  conf.gem github: 'shugo/mruby-bin-picoruby', branch: 'for_prk_firmware_0.9.18'
  conf.gem github: 'shugo/mruby-bin-picoirb', branch: 'for_prk_firmware_0.9.18'

  conf.cc.defines << "DISABLE_MRUBY"
  if ENV["PICORUBY_DEBUG_BUILD"]
    conf.cc.defines << "PICORUBY_DEBUG"
    conf.cc.flags.flatten!
    conf.cc.flags.reject! { |f| %w(-g -O3).include? f }
    conf.cc.flags << "-g3"
    conf.cc.flags << "-O0"
  else
    conf.cc.defines << "NDEBUG"
  end
  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"
  conf.cc.defines << "MRBC_USE_MATH"
  conf.cc.defines << "MRBC_INT64"
  conf.cc.defines << "MAX_SYMBOLS_COUNT=#{ENV['MAX_SYMBOLS_COUNT'] || 700}"

  conf.cc.defines << ENV.keys.find { |env|
    env.start_with? "MRBC_USE_HAL"
  }.then { |hal|
    if hal.nil?
      "MRBC_USE_HAL_POSIX"
    elsif hal == "MRBC_USE_HAL"
      "#{hal}=#{ENV[hal]}"
    elsif hal.start_with?("MRBC_USE_HAL_")
      hal
    else
      raise "Invalid MRBC_USE_HAL_ definition!"
    end
  }

end
