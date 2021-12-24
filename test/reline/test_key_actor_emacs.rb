require_relative 'helper'

class Reline::KeyActor::Emacs::Test < Reline::TestCase
  def setup
    Reline.send(:test_mode)
    @prompt = '> '
    @config = Reline::Config.new # Emacs mode is default
    @config.autocompletion = false
    Reline::HISTORY.instance_variable_set(:@config, @config)
    Reline::HISTORY.clear
    @encoding = Reline::IOGate.encoding
    @line_editor = Reline::LineEditor.new(@config, @encoding)
    @line_editor.reset(@prompt, encoding: @encoding)
  end

  def teardown
    Reline.test_reset
  end

  def test_halfwidth_kana_width_dakuten
    input_keys('ｶﾞｷﾞｹﾞｺﾞ')
    assert_byte_pointer_size('ｶﾞｷﾞｹﾞｺﾞ')
  end
end
