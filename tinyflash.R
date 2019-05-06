library(ggplot2)
library(data.table)
library(foreach)
library(scales)
library(RColorBrewer)

ggplot <- function(...) { ggplot2::ggplot(...) + theme_bw() }

options(width=Sys.getenv("COLUMNS"))

tmp1 <- data.table(read.table("run.log"))
tmp2 <- data.table(read.table("run3.log"))
tmp <- rbind(tmp1,tmp2)

tmp[,totcs:=V3*V4]

setkey(tmp,"totcs","V2","V3")

tmp

tmp[,pol:=ifelse(V4==4096,"4K-bucket-eviction","global eviction")]

pl <- ggplot(tmp,aes(totcs,V5,color=pol,shape=pol))+
    geom_point(size=2)+
    geom_line()+
    scale_color_brewer("",palette="Set1")+
    scale_shape_discrete("")+
    scale_x_continuous("Cache Size",expand=c(0,0))+
    scale_y_continuous("Miss Ratio",expand=c(0,0))+
    theme(legend.key.width = unit(0.4, "cm"),
          legend.key.height = unit(0.5, "cm"),
          plot.margin = unit(c(0.2, 0.8, 0.4, 0.4), "lines"),
          legend.text = element_text(size = rel(1.1)),
          legend.position=c(0.5,0.5),
          axis.title.x = element_text(size = rel(1.1),vjust=-.1),axis.title.y = element_text(size = rel(1.1),vjust=1.2)
          )+
    coord_cartesian(xlim=c(0,2^35),ylim=c(0,1))

oname <- paste("/tmp/sim.pdf")
pdf(oname,11,5) ## changed
print(pl)
dev.off()

