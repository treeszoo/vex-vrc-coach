#!/bin/bash

# Script to increment the patch version in project.pros
# Usage: ./increment_version.sh

PROJECT_FILE="project.pros"

# Check if project.pros exists
if [ ! -f "$PROJECT_FILE" ]; then
    echo "Error: $PROJECT_FILE not found!"
    exit 1
fi

# Use Perl for reliable cross-platform regex replacement (supports with or without "v")
perl -i -pe 's/("project_name": ".*?v?\d+\.\d+\.)(\d+)(")/my $new = $2 + 1; "$1$new$3"/e' "$PROJECT_FILE"

# Display the new version with timestamp
NEW_VERSION=$(grep '"project_name":' "$PROJECT_FILE" | sed 's/.*"project_name": "\(.*\)".*/\1/')
CURRENT_TIME=$(date '+%H:%M:%S')
echo "[$CURRENT_TIME] Version updated to: $NEW_VERSION"
