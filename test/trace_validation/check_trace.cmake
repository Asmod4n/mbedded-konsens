# Checks one trace against model/tla/TraceSimulation.tla with TLC.
# Variables: JAVA, TLA2TOOLS_JAR, MODEL_DIRECTORY, TRACE_FILE, WORK_DIRECTORY.
# The trace is accepted when TLC reports the invariant NotDone as violated.
file(REMOVE_RECURSE ${WORK_DIRECTORY})
file(MAKE_DIRECTORY ${WORK_DIRECTORY})
foreach(f SequencePaxos.tla TraceSimulation.tla TraceSimulation.cfg)
    file(COPY ${MODEL_DIRECTORY}/${f} DESTINATION ${WORK_DIRECTORY})
endforeach()
file(COPY_FILE ${TRACE_FILE} ${WORK_DIRECTORY}/TraceData.tla)
execute_process(
    COMMAND ${JAVA} -XX:+UseParallelGC -Xmx700m -cp ${TLA2TOOLS_JAR} tlc2.TLC -workers 1
            -metadir ${WORK_DIRECTORY}/states -config TraceSimulation.cfg TraceSimulation.tla
    WORKING_DIRECTORY ${WORK_DIRECTORY}
    OUTPUT_VARIABLE tlc_output
    ERROR_VARIABLE tlc_output)
file(WRITE ${WORK_DIRECTORY}/tlc.log "${tlc_output}")
string(REGEX MATCHALL "<<\"reached\", [0-9]+, [0-9]+>>" reached "${tlc_output}")
list(POP_BACK reached last_reached)
if(NOT tlc_output MATCHES "Invariant NotDone is violated")
    message(FATAL_ERROR "TLC refused ${TRACE_FILE}: the last matched line is ${last_reached}. "
                        "Log: ${WORK_DIRECTORY}/tlc.log")
endif()
file(REMOVE_RECURSE ${WORK_DIRECTORY}/states)
message(STATUS "TLC accepted ${TRACE_FILE}: ${last_reached}")
