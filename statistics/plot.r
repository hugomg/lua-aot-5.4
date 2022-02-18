library(readr)
library(dplyr)
library(ggplot2)
library(xtable)

bench_codes <- c(
  "binarytrees",
  "fannkuch",
  "fasta",
  "knucleotide",
  "mandelbrot",
  "nbody",
  "spectralnorm",
  "cd",
  "deltablue",
  "havlak",
  "json",
  "list",
  "permute",
  "richards",
  "towers"
)

bench_names <- c(
  "Binary Trees",
  "Fannkuch",
  "Fasta",
  "K-Nucleotide",
  "Mandelbrot",
  "N-Body",
  "Spectral Norm",
  "CD",
  "Deltablue",
  "Havlak",
  "Json",
  "List",
  "Permute",
  "Richards",
  "Hanoi Towers"
)

impl_codes <- c("lua", "trm", "aot", "jit", "jof", "fun", "ctx")
#impl_used <- c("lua", "trm", "aot", "jit", "fun", "ctx")
#impl_names <- c("Lua", "Trampoline", "LuaAOT","LuaJIT", "Function", "Struct")
impl_used <- c("lua", "aot", "jit")
impl_names <- c("Lua", "LuaAOT","LuaJIT")

data <- read_csv("times-slow.csv", col_types = cols(
    Benchmark = col_factor(bench_codes),
    Implementation = col_factor(impl_codes),
    N = col_integer(),
    Time = col_double()
))

plot_data <- filter(data, Implementation %in% impl_used)

aggr_data <- data %>%
  group_by(Benchmark,Implementation) %>%
  summarize(
    meanTime=mean(Time),
    d=max(max(Time)-meanTime, meanTime-min(Time)))

mean_times <- plot_data %>%
    group_by(Benchmark,Implementation) %>%
    summarize(Time=mean(Time)) %>%
    ungroup()

mean_times_lua <- mean_times %>%
      filter(Implementation=="lua") %>%
      select(Benchmark, LuaTime=Time)

normal_times <- plot_data %>%
    inner_join(mean_times_lua, by=c("Benchmark")) %>%
    mutate(Time=Time/LuaTime) %>%
    group_by(Benchmark,Implementation) %>%
    summarize(
      mean_time = mean(Time),
      sd_time  = sd(Time),
      min_time = min(Time),
      max_time = max(Time)) %>%
    ungroup()
    
dodge <- position_dodge(0.9)
ggplot(normal_times,
       aes(x=Benchmark,y=mean_time,fill=Implementation)) +
  geom_col(position=dodge) +
  geom_errorbar(aes(ymin=min_time, ymax=max_time), position=dodge, width=.3)+
  xlab("Benchmark")+
  ylab("Time")+
  scale_fill_discrete(labels=impl_names) +
  scale_x_discrete(labels=bench_names) +
  scale_y_continuous(limits=c(0.0, 1.2), breaks=seq(from=0.0,to=1.2,by=0.25) ) +
  theme(axis.text.x=element_text(angle=30, hjust=1), legend.position = "top",)

##########

trm_times <- data %>%
  filter(Implementation %in% c("trm", "cor")) %>%
  group_by(Benchmark,Implementation) %>%
  summarize(Time=mean(Time)) %>%
  ungroup()

trm_ratio <- inner_join(
  filter(trm_times, Implementation=="trm"),
  filter(trm_times, Implementation=="cor"),
  by=c("Benchmark")) %>%
mutate(Ratio=100.0*Time.x/Time.y) %>%
select(Benchmark, Ratio)
