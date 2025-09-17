sim.setSpeedLimit(1000.0);
var motes = sim.getMotes();
for (var i = 0; i < motes.length; i++) {
  var x = Math.random() * 300; // adjust range as needed
  var y = Math.random() * 300; // adjust range as needed
  motes[i].getInterfaces().getPosition().setCoordinates(x, y, 0);
  var mote_id = motes[i].getID();
  log.log(time + "\tID:"+ mote_id + "\tx:" + x + "\ty:" + y + "\n");
}
TIMEOUT(7400000, log.testOK());
//TIMEOUT(2000000, log.testOK());
while (true) {
  log.log(time + "\tID:" + id + "\t" + msg + "\n");
  YIELD();
}