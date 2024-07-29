using HawkesProcesses, Distributions, Plots

#const bg = 0.5
#const kappa = 0.5
#const maxT = 100

struct HawkesGenerator
    bg::Number
    kappa::Number
    maxT::Number
end

struct HawkesSimulation
    ts::Vector{Float64}
    intensity::Vector{Number}
end

hawkesKernel(x) = pdf.(Distributions.Exponential(1/0.5), x)

function simulate(gen::HawkesGenerator)::HawkesSimulation
    simevents = HawkesProcesses.simulate(gen.bg, gen.kappa, hawkesKernel, gen.maxT)
    ts = collect(0:0.1:gen.maxT)
    intensity = HawkesProcesses.intensity(ts, simevents, gen.bg, gen.kappa, hawkesKernel)
    return HawkesSimulation(ts, intensity)
end

# HawkesSimulation.intensity has the intensity multipliers across the timestamps
# in ts. They're not normalized.
#
# A great idea would be a function that normalizes it and prints to a file, so
# we can read from C to actually create the sender program.

function plotHawkesSimulation(sim::HawkesSimulation)
    plot(sim.ts, sim.intensity, xlabel = "Time", ylabel = "Intensity", label = "")
end
