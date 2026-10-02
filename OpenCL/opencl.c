vel = calculateProbabilityFlow(...);
temp_pos = pos + vel * dt;
new_phi = atan2(temp_pos.z, temp_pos.x);
pos = sphericalToCartesian(r, theta, new_phi);