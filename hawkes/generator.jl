using HawkesProcesses, Distributions, Plots

const T = 200

#const bg = 0.5
#const kappa = 0.5
struct HawkesGenerator
    bg::Number
    kappa::Number
end

hawkesKernel(x) = pdf.(Distributions.Exponential(1/0.5), x)

function simulate(gen::HawkesGenerator)::Vector{Number}
    simevents = HawkesProcesses.simulate(gen.bg, gen.kappa, hawkesKernel, T)
    ts = collect(0:0.1:T)
    intensity = HawkesProcesses.intensity(ts, simevents, gen.bg, gen.kappa, hawkesKernel)
    return intensity
end

# intensity has the intensity multipliers across the timestamps. They're not
# normalized.
#
# A great idea would be a function that normalizes it and prints to a file, so
# we can read from C to actually create the sender program.

function plotHawkesSimulation(intensity::Vector{Number})
    plot(collect(0:0.1:T), intensity, xlabel = "Time", ylabel = "Intensity", label = "")
end

simulateAndPlot(bg::Number, kappa::Number) = plotHawkesSimulation(simulate(HawkesGenerator(bg, kappa)))

intensity = simulate(HawkesGenerator(0.5, 0.5))

for i in intensity
    print(i, " ")
end
println()
