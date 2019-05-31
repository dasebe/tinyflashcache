library(data.table)

dt <- 1/(1:1e6)^0.9

head(dt)

ts <- sum(dt)

sum(head(dt,n=1e5))/ts

sum(tail(dt,n=1e5))/ts

(1-tail(dt,n=1)/ts)^1e9

tail(dt,n=1)/ts*100000000

1/30*1/160753*100000000


log2(1e6)

dt[1]/ts

sum(dt[(2^26):(2^27)])/ts

sum(tail(dt,n=2^27))/ts
