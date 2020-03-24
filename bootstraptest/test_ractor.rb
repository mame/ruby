# A return value of a Ractor block will be a message from the Ractor.
assert_equal 'ok', %q{
  # join
  r = Ractor.new do
    'ok'
  end
  r.recv
}

# Passed arguments to Ractor.new will be a block parameter
# The values are passed with Ractor-communication pass.
assert_equal 'ok', %q{
  # ping-pong with arg
  r = Ractor.new 'ok' do |msg|
    msg
  end
  r.recv
}

assert_equal 'ok', %q{
  # ping-pong with two args
  r =  Ractor.new 'ping', 'pong' do |msg, msg2|
    [msg, msg2]
  end
  'ok' if r.recv == ['ping', 'pong']
}

# Ractor#send passes an object with copy to a Ractor
# and Ractor.recv in the Ractor block can receive the passed value.
assert_equal 'ok', %q{
  # ping-pong with channel
  r = Ractor.new do
    msg = Ractor.recv
  end
  r.send 'ok'
  r.recv
}

# Ractor.select(*channels) receives a values from a channel.
# It is similar to select(2) and Go's select syntax.
# The return value is [ch, received_value]
assert_equal 'ok', %q{
  # select 1
  r1 = Ractor.new{'r1'}
  r, obj = Ractor.select(r1)
  'ok' if r == r1 and obj == 'r1'
}

assert_equal '["r1", "r2"]', %q{
  # select 2
  r1 = Ractor.new{'r1'}
  r2 = Ractor.new{'r2'}
  rs = [r1, r2]
  as = []
  r, obj = Ractor.select(*rs)
  rs.delete(r)
  as << obj
  r, obj = Ractor.select(*rs)
  as << obj
  as.sort #=> ["r1", "r2"]
}

assert_equal 'true', %q{
  def test n
    rs = (1..n).map do |i|
      Ractor.new(i) do |i|
        "r#{i}"
      end
    end
    as = []
    all_rs = rs.dup

    n.times{
      r, obj = Ractor.select(*rs)
      as << [r, obj]
      rs.delete(r)
    }

    if as.map{|r, o| r.inspect}.sort == all_rs.map{|r| r.inspect}.sort &&
       as.map{|r, o| o}.sort == (1..n).map{|i| "r#{i}"}.sort
      'ok'
    else
      'ng'
    end
  end

  30.times.map{|i|
    test i
  }.all?('ok')
}

# communication channels belong to a Ractor will be closed
# if the Ractor is terminated.
assert_equal 'ok', %q{
  # closed-channel (Ractor)
  r = Ractor.new do
    'finish'
  end
  r.recv
  begin
    o = r.recv
  rescue Ractor::ClosedError
    'ok'
  else
    "ng: #{o}"
  end
}

assert_equal 'ok', %q{
  r = Ractor.new do
  end

  r.recv # closed

  begin
    r.send(1)
  rescue Ractor::ClosedError
    'ok'
  else
    'ng'
  end
}

# multiple Ractors can recv (wait) from one Ractor
assert_equal '[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]', %q{
  pipe = Ractor.new do
    loop do
      Ractor.send Ractor.recv
    end
  end

  RN = 10
  rs = RN.times.map{|i|
    Ractor.new pipe, i do |pipe, i|
      msg = pipe.recv
      msg # ping-pong
    end
  }
  RN.times{|i|
    pipe << i
  }
  RN.times.map{
    r, n = Ractor.select(*rs)
    rs.delete r
    n
  }.sort
}

# multiple Ractors can send to one Ractor
assert_equal '[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]', %q{
  pipe = Ractor.new do
    loop do
      Ractor.send Ractor.recv
    end
  end

  RN = 10
  rs = RN.times.map{|i|
    Ractor.new pipe, i do |pipe, i|
      pipe << i
    end
  }
  RN.times.map{
    pipe.recv
  }.sort
}

# an exception in a Ractor will be re-raised at Ractor#recv
assert_equal '[RuntimeError, "ok", true]', %q{
  r = Ractor.new do
    raise 'ok' # exception will be transferred receiver
  end
  begin
    r.recv
  rescue Ractor::RemoteError => e
    [e.cause.class,   #=> RuntimeError
     e.cause.message, #=> 'ok'
     e.ractor == r]   #=> true
  end
}

# unshareable object are copied
assert_equal 'false', %q{
  obj = 'str'.dup
  r = Ractor.new obj do |msg|
    msg.object_id
  end
  
  obj.object_id == r.recv
}

# To copy the object, now Marshal#dump is used
assert_equal 'no _dump_data is defined for class Thread', %q{
  obj = Thread.new{}
  begin
    r = Ractor.new obj do |msg|
      msg
    end
  rescue TypeError => e
    e.message #=> no _dump_data is defined for class Thread
  else
    'ng'
  end
}

# send sharable and unsharable objects
assert_equal "[[[1, true], [:sym, true], [:xyzzy, true], [\"frozen\", true], " \
             "[(3/1), true], [(3+4i), true], [/regexp/, true], [C, true]], " \
             "[[\"mutable str\", false], [[:array], false], [{:hash=>true}, false]]]", %q{
  r = Ractor.new do
    while v = Ractor.recv
      Ractor.send v
    end
  end

  class C
  end

  sharable_objects = [1, :sym, 'xyzzy'.to_sym, 'frozen'.freeze, 1+2r, 3+4i, /regexp/, C]

  sr = sharable_objects.map{|o|
    r << o
    o2 = r.recv
    [o, o.object_id == o2.object_id]
  }

  ur = unsharable_objects = ['mutable str'.dup, [:array], {hash: true}].map{|o|
    r << o
    o2 = r.recv
    [o, o.object_id == o2.object_id]
  }
  [sr, ur].inspect
}

# move example2: String
# touching moved object causes an error
assert_equal 'hello world', %q{
  # move
  r = Ractor.new do
    obj = Ractor.recv
    obj << ' world'
  end

  str = 'hello'
  r.move str
  modified = r.recv

  begin
    str << ' exception' # raise Ractor::MovedError
  rescue Ractor::MovedError
    modified #=> 'hello world'
  else
    raise 'unreachable'
  end
}

# move example2: Array
assert_equal '[0, 1]', %q{
  r = Ractor.new do
    ary = Ractor.recv
    ary << 1
  end

  a1 = [0]
  r.move a1
  a2 = r.recv
  begin
    a1 << 2 # raise Ractor::MovedError
  rescue Ractor::MovedError
    a2.inspect
  end
}

# Access to global-variables are prohibitted
assert_equal 'can not access global variables from non-main Ractors', %q{
  $gv = 1
  r = Ractor.new do
    $gv
  end

  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# Access to global-variables are prohibitted
assert_equal 'can not access global variables from non-main Ractors', %q{
  r = Ractor.new do
    $gv = 1
  end

  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# selfs are different objects
assert_equal 'false', %q{
  r = Ractor.new do
    self.object_id
  end
  r.recv == self.object_id #=> false
}

# self is a Ractor instance
assert_equal 'true', %q{
  r = Ractor.new do
    self.object_id
  end
  r.object_id == r.recv #=> true
}

# given block Proc will be isolated, so can not access outer variables.
assert_equal 'ArgumentError', %q{
  begin
    a = true
    r = Ractor.new do
      a
    end
  rescue => e
    e.class
  end
}

# ivar in sharable-objects are not allowed to access from non-main Ractor
assert_equal 'can not access instance variables of classes/modules from non-main Ractors', %q{
  class C
    @iv = 'str'
  end

  r = Ractor.new do
    class C
      p @iv
    end
  end


  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# ivar in sharable-objects are not allowed to access from non-main Ractor
assert_equal 'can not access instance variables of shareable objects from non-main Ractors', %q{
  shared = Ractor.new{}
  shared.instance_variable_set(:@iv, 'str')

  r = Ractor.new shared do |shared|
    p shared.instance_variable_get(:@iv)
  end

  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# cvar in sharable-objects are not allowed to access from non-main Ractor
assert_equal 'can not access class variables from non-main Ractors', %q{
  class C
    @@cv = 'str'
  end

  r = Ractor.new do
    class C
      p @@cv
    end
  end


  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# Getting non-sharable objects via constants by other Ractors is not allowed
assert_equal 'can not access non-sharable objects in constant CONST by non-main Ractors', %q{
  class C
    CONST = 'str'
  end
  r = Ractor.new do
    C::CONST
  end
  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# Setting non-sharable objects into constants by other Ractors is not allowed
assert_equal 'can not set constants with non-shareable objects by non-main Ractors', %q{
  class C
  end
  r = Ractor.new do
    C::CONST = 'str'
  end
  begin
    r.recv
  rescue Ractor::RemoteError => e
    e.cause.message
  end
}

# A Ractor can have a name
assert_equal 'test-name', %q{
  r = Ractor.new name: 'test-name' do
  end
  r.name
}

# If Ractor doesn't have a name, Ractor#name returns nil.
assert_equal 'nil', %q{
  r = Ractor.new do
  end
  r.name.inspect
}