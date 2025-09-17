sim.setSpeedLimit(1000.0);
TIMEOUT(7400000, log.testOK());

var motes = sim.getMotes();
for (var i = 0; i < motes.length; i++) {
  var x = motes[i].getInterfaces().getPosition().getXCoordinate();
  var y = motes[i].getInterfaces().getPosition().getYCoordinate();
  var mote_id = motes[i].getID();
  log.log(time + "\tID:"+ mote_id + "\tx:" + x + "\ty:" + y + "\n");
}

while (true) {
  log.log(time + "\tID:" + id + "\t" + msg + "\n");
  YIELD();
}