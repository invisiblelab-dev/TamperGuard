"""
Compression Benchmark Visualization
Generates scatter plots showing compression ratio vs speed trade-offs
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import sys
import os
from pathlib import Path

def load_results(csv_path):
    """Load benchmark results from CSV"""
    try:
        df = pd.read_csv(csv_path)
        required_columns = ['File', 'Config', 'Compression_Ratio', 'Write_Time_s', 'Read_Time_s']
        if not all(col in df.columns for col in required_columns):
            raise ValueError("Missing required columns")
        return df
    except Exception as e:
        print(f"Invalid CSV format: {e}")
        sys.exit(1)

def create_scatter_plot(df, file_name, metric, output_dir):
    """Create scatter plot for compression ratio vs execution time"""
    
    # Filter data for specific file
    file_data = df[df['File'] == file_name].copy()
    
    if file_data.empty:
        print(f"No data found for file: {file_name}")
        return
    
    # Set up the plot
    plt.figure(figsize=(10, 6))
    
    # Define colors for different algorithm families
    colors = {
        'no_compression': '#666666',
        'lz4_ultra_fast': '#ff6b6b', 
        'lz4_default': '#ff9999',
        'lz4_high': '#ffcccc',
        'zstd_ultra_fast': '#4ecdc4',
        'zstd_default': '#45b7aa',
        'zstd_high': '#2d7a73'
    }
    
    # Plot points for each configuration
    for _, row in file_data.iterrows():
        config = row['Config']
        ratio = row['Compression_Ratio']
        execution_time = row[metric]  # Raw time in seconds
        
        color = colors.get(config, '#000000')
        plt.scatter(execution_time, ratio, 
                   c=color, s=80, alpha=0.8, 
                   label=config.replace('_', ' ').title(),
                   edgecolors='black', linewidth=0.5)
        
        # Add config name near point
        plt.annotate(config.replace('_', ' '), 
                    (execution_time, ratio),
                    xytext=(5, 5), textcoords='offset points',
                    fontsize=8, alpha=0.7)
    
    # Customize plot
    operation = metric.replace('_Time_s', '').replace('_', ' ').title()
    plt.xlabel(f'{operation} Execution Time (seconds)')
    plt.ylabel('Compression Ratio')
    plt.title(f'{file_name.title()} - Compression Ratio vs {operation} Time')
    plt.grid(True, alpha=0.3)
    
    # Set logarithmic scale for better visualization of time differences
    # plt.xscale('log')

    # Use StrMethodFormatter for automatic decimal formatting
    from matplotlib.ticker import StrMethodFormatter
    plt.gca().xaxis.set_major_formatter(StrMethodFormatter('{x:.3f}s'))
    
    # Remove duplicate labels
    handles, labels = plt.gca().get_legend_handles_labels()
    by_label = dict(zip(labels, handles))
    plt.legend(by_label.values(), by_label.keys(), 
              bbox_to_anchor=(1.05, 1), loc='upper left')
    
    plt.tight_layout()
    
    # Save plot
    metric_name = metric.replace('_Time_s', '').replace('_', '_')
    output_file = output_dir / f'{file_name}_{metric_name}_time_vs_ratio.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Created: {output_file}")

def create_summary_plot(df, metric, output_dir):
    """Create summary plot with all files"""
    plt.figure(figsize=(14, 8))
    
    files = df['File'].unique()
    configs = df['Config'].unique()
    
    # Colors for different files
    colors_by_file = plt.cm.Set3(np.linspace(0, 1, len(files)))
    
    # Markers for different compression configs
    markers = {
        'no_compression': 'o',
        'lz4_ultra_fast': '^', 
        'lz4_default': 's',
        'lz4_high': 'D',
        'zstd_ultra_fast': 'v',
        'zstd_default': 'p',
        'zstd_high': '*'
    }
    
    # Plot each file-config combination
    for i, file_name in enumerate(files):
        file_data = df[df['File'] == file_name]
        
        for _, row in file_data.iterrows():
            config = row['Config']
            ratio = row['Compression_Ratio'] 
            execution_time = row[metric]
            
            marker = markers.get(config, 'o')
            color = colors_by_file[i]
            
            plt.scatter(execution_time, ratio,
                       c=[color], s=80, alpha=0.8,
                       marker=marker,
                       edgecolors='black', linewidth=0.5,
                       label=f"{file_name}" if config == list(file_data['Config'])[0] else "")
    
    # Create custom legend for compression algorithms
    legend_elements = []
    for config, marker in markers.items():
        legend_elements.append(plt.Line2D([0], [0], marker=marker, color='gray', 
                                        linestyle='None', markersize=8,
                                        label=config.replace('_', ' ').title()))
    
    operation = metric.replace('_Time_s', '').replace('_', ' ').title()
    plt.xlabel(f'{operation} Execution Time (seconds)')
    plt.ylabel('Compression Ratio')
    plt.title(f'All Files - Compression Ratio vs {operation} Time')
    plt.grid(True, alpha=0.3)
    
    # Customize x-axis ticks to show actual time values instead of scientific notation
    def time_formatter(x, pos):
        if x < 0.001:
            return f'{x*1000:.1f}ms'
        elif x < 1:
            return f'{x:.3f}s'
        else:
            return f'{x:.1f}s'

    plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(time_formatter))
    
    # Create two legends: one for files (colors), one for algorithms (shapes)
    file_legend = plt.legend(bbox_to_anchor=(1.02, 1), loc='upper left', title='Files')
    algo_legend = plt.legend(handles=legend_elements, bbox_to_anchor=(1.02, 0.6), 
                            loc='upper left', title='Algorithms')
    plt.gca().add_artist(file_legend)  # Add back the file legend
    
    plt.tight_layout()
    
    metric_name = metric.replace('_Time_s', '').replace('_', '_')
    output_file = output_dir / f'all_files_{metric_name}_time_vs_ratio.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Created: {output_file}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 plot_results.py <csv_file>")
        print("Example: python3 plot_results.py benchmark_results.csv")
        sys.exit(1)
    
    csv_path = Path(sys.argv[1]).resolve()
    if not csv_path.is_relative_to(Path.cwd()):
        print("Error: Path must be within current directory")
        sys.exit(1)
    
    # Load data
    df = load_results(csv_path)
    
    # Create output directory
    output_dir = csv_path.parent / 'plots'
    output_dir.mkdir(exist_ok=True)
    
    # Get unique files
    files = df['File'].unique()
    
    print(f"Creating plots for {len(files)} files...")
    
    # Create individual file plots
    for file_name in files:
        print(f"\nProcessing {file_name}:")
        create_scatter_plot(df, file_name, 'Write_Time_s', output_dir)
        create_scatter_plot(df, file_name, 'Read_Time_s', output_dir)
    
    # Create summary plots
    print(f"\nCreating summary plots:")
    create_summary_plot(df, 'Write_Time_s', output_dir)
    create_summary_plot(df, 'Read_Time_s', output_dir)
    
    print(f"\n✅ All plots saved to: {output_dir}")
    print(f"📊 Total plots created: {len(files) * 2 + 2}")

if __name__ == "__main__":
    main()
