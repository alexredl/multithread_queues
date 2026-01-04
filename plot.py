#!/bin/python3

from os import listdir, makedirs
from dataclasses import dataclass
import matplotlib.pyplot as plt

dir_data = 'data'
dir_plots = 'plots'

program_seq = 'seq'
program_cas = 'cas'

colors = ['#EA558F', '#F39345', '#FEC927', '#9BC437', '#00AD91', '#00ADC1', '#0084C2', '#006699', '#685DA4', '#9A5A9F', '#BD4492', '#CB2649']

show = False  # show instead of plot as pdf

makedirs(dir_data, exist_ok=True)
makedirs(dir_plots, exist_ok=True)
logfiles = listdir(dir_data)

cm_inch = lambda cm: cm / 2.54

def get_program(filename: str) -> str:
  return filename.split('_')[1]

def get_threads(filename: str) -> int:
  if get_program(filename) == 'seq':
    return 1
  return int(filename.split('_')[2][1:])

def get_duration(filename: str) -> int:
  if get_program(filename) == 'seq':
    return int(filename.split('_')[2][1:])
  return int(filename.split('_')[3][1:])

def get_batch(filename: str) -> int:
  if get_program(filename) == 'seq':
    return int(filename.split('_')[3][1:].split('.')[0])
  return int(filename.split('_')[4][1:])

def get_pattern(filename: str) -> str:
  if get_program(filename) == 'seq':
    return ''
  return filename.split('_')[5].split('.')[0]

patterns = []
batches = []
durations = []
programs = []
threads = []
for logfile in logfiles:
  pattern = get_pattern(logfile)
  if pattern and pattern not in patterns:
    patterns.append(pattern)
  batch = get_batch(logfile)
  if batch not in batches:
    batches.append(batch)
  duration = get_duration(logfile)
  if duration not in durations:
    durations.append(duration)
  program = get_program(logfile)
  if program != program_seq and program not in programs:
    programs.append(program)
  thread = get_threads(logfile)
  if thread not in threads:
    threads.append(thread) 
patterns.sort()
batches.sort()
durations.sort()
programs.sort()
threads.sort()

@dataclass
class Stats:
  filename: str = 0
  threads: int = 0
  exp_duration: float = 0
  batchsize: int = 0
  repetitions: int = 0
  enques: list[int] = None
  deques: list[int] = None
  duration: float = 0
  enq_succ: float = 0
  enq_fail: float = 0
  deq_succ: float = 0
  deq_fail: float = 0
  freelist_insert: float = 0
  freelist_max: int = 0
  cas_succ: float = 0
  cas_fail: float = 0

  @property
  def throughput(self) -> float:
    return (self.enq_succ + self.deq_succ) / self.duration

  @property
  def throughput_all(self) -> float:
    return (self.enq_succ + self.enq_fail + self.deq_succ + self.deq_fail) / self.duration

  @property
  def cas_succ_rate(self) -> float:
    return self.cas_succ / (self.cas_succ + self.cas_fail)

  def __lt__(self, other) -> bool:
    return self.threads < other.threads

  @classmethod
  def file(cls, filename: str):
    stats = cls()
    stats.filename = filename
    stats.threads = get_threads(filename)
    stats.exp_duration = get_duration(filename)
    stats.batchsize = get_batch(filename)

    stats_counter = 0

    with open(f'{dir_data}//{filename}', 'r') as file:
      threads = int(file.readline().split()[-1])
      if threads != stats.threads:
        raise ValueError(f'Something is off: threads in filename ({stats.threads}) != threads in file ({threads})')
      duration = int(file.readline().split()[-1])
      if duration != stats.exp_duration:
        raise ValueError(f'Something is off: duration in filename ({stats.exp_duration}) != duration in file ({duration})')
      stats.repetitions = int(file.readline().split()[-1])
      enques = file.readline()
      if '[' in enques:
        stats.enques = [int(e) for e in enques.split('[')[-1].split(']')[0].split()]
      else:
        stats.enques = [int(enques.split()[-1])]
      deques = file.readline()
      if '[' in deques:
        stats.deques = [int(d) for d in deques.split('[')[-1].split(']')[0].split()]
      else:
        stats.deques = [int(deques.split()[-1])]

      lines = file.readlines()
    for i, line in enumerate(lines):
      if line.strip() != 'Summary STATS:':
        continue
      stats_counter += 1
      stats.duration += float(lines[i+1].split()[1])
      stats.enq_succ += int(lines[i+2].split()[-1])
      stats.enq_fail += int(lines[i+3].split()[-1])
      stats.deq_succ += int(lines[i+4].split()[-1])
      stats.deq_fail += int(lines[i+5].split()[-1])
      stats.freelist_insert += int(lines[i+6].split()[-1])
      stats.freelist_max = max(stats.freelist_max, int(lines[i+7].split()[-1])) 
      stats.cas_succ += int(lines[i+8].split()[-1])
      stats.cas_fail += int(lines[i+9].split()[-1])

    if stats_counter != stats.repetitions:
      raise ValueError(f'Something is off: repetitions ({stats.repetitions}) != reportet summaries ({stats_counter})')

    stats.duration /= stats.repetitions
    stats.enq_succ /= stats.repetitions
    stats.enq_fail /= stats.repetitions
    stats.deq_succ /= stats.repetitions
    stats.deq_fail /= stats.repetitions
    stats.freelist_insert /= stats.repetitions
    stats.cas_succ /= stats.repetitions
    stats.cas_fail /= stats.repetitions

    return stats

for batch in batches:
  for duration in durations:
    logfiles_filtered = [logfile for logfile in logfiles if get_batch(logfile) == batch and get_duration(logfile) == duration]
    if not logfiles_filtered:
      print(f'No logfiles for batch "{batch}" and duration "{duration}". Skipping.')
      continue
    logfiles_seq = [logfile for logfile in logfiles_filtered if get_program(logfile) == program_seq]
    if not logfiles_seq:
      print(f'No sequential logfiles for batch "{batch}" and duration "{duration}". Skipping.')
      continue
    stats_seq = Stats.file(logfiles_seq[0])
    stats_conc = {}
    for program in programs:
      stats_program = {}
      for pattern in patterns:
        stats_pattern = []
        for logfile in logfiles_filtered:
          if get_program(logfile) == program and get_pattern(logfile) == pattern:
            stats_pattern.append(Stats.file(logfile))
        stats_pattern.sort()
        stats_program[pattern] = stats_pattern
      stats_conc[program] = stats_program

    # plot throughput (all successfull ops)
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      axs[i].hlines(stats_seq.throughput, 1, max(threads), colors=colors[0], label=program_seq)
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.throughput for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_yscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Throughput [succ. ops / s]')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//throughput_t{duration}_b{batch}.pdf')
      plt.close(fig)

    """
    # plot throughput (all ops)
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      axs[i].hlines(stats_seq.throughput_all, 1, max(threads), colors=colors[0], label=program_seq)
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.throughput_all for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_yscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Throughput [succ. ops / s]')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//throughput_all_t{duration}_b{batch}.pdf')
      plt.close(fig)
    """

    # plot speedup (all successfull ops)
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.throughput / stats_seq.throughput for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_yscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Speedup')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//speedup_t{duration}_b{batch}.pdf')
      plt.close(fig)

    """
    # plot speedup (all ops)
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.throughput_all / stats_seq.throughput_all for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_yscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Speedup')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//speedup_all_t{duration}_b{batch}.pdf')
      plt.close(fig)
    """

    # plot dequeue fails
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.deq_fail for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Dequeue fails')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//deq_fails_t{duration}_b{batch}.pdf')
      plt.close(fig)

    # plot freelist insterts
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.freelist_insert for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_yscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Freelist inserts')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//freelist_insert_t{duration}_b{batch}.pdf')
      plt.close(fig)

    # plot freelist max
    fig, axs = plt.subplots(1, len(patterns), figsize=(cm_inch(5 * len(patterns)), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for i, pattern in enumerate(patterns):
      axs[i].set_title(f'Pattern: {pattern}')
      for j, program in enumerate(programs, 1):
        stats = stats_conc[program][pattern]
        axs[i].plot([s.threads for s in stats], [s.freelist_max for s in stats], color=colors[j], marker='x', label=program)
        axs[i].set_xlabel('Threads')
        axs[i].set_xscale('log')
        axs[i].set_yscale('log')
        axs[i].set_xlim((1, max(threads)))
    axs[0].legend(fontsize=7)
    axs[0].set_ylabel('Freelist max length')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//freelist_max_t{duration}_b{batch}.pdf')
      plt.close(fig)

    # plot cas success rate
    fig, ax = plt.subplots(1, 1, figsize=(cm_inch(5), cm_inch(6)))
    if len(patterns) == 1:
      axs = [axs]
    for j, pattern in enumerate(patterns, 5):
      stats = stats_conc[program_cas][pattern]
      ax.plot([s.threads for s in stats], [s.cas_succ_rate for s in stats], color=colors[j], marker='x', label=pattern)
    ax.set_xlabel('Threads')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlim((1, max(threads)))
    ax.legend(fontsize=7)
    ax.set_ylabel('CAS success rate')
    plt.tight_layout()
    if show:
      plt.show()
    else:
      plt.savefig(f'{dir_plots}//cas_succ_t{duration}_b{batch}.pdf')
      plt.close(fig)

