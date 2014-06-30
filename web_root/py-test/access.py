####
#
#
# Access to log files.
# 
# Path is /mnt/audio-stor/log/<year>/<month>/<day>/<N>-second.mp3
#
# simple structure to join seconds.
# (join <N>[-,]<M>)+
#  -: join seconds N to M, defined for N > M
#  ,: join secoinds N and M, defined for N,M
# ex:
#
# join 1000-2000
# join 1-2
#
# All of th joins are output in order, so a new command should be executed to generate more data
# 
# TODO
# random command to select random second from a cache file
# cache of all seconds in fixed-record-size file
# command to update cache (recache)
# periodic expiry of cache file (configurable, default > 6h)
# 
###

