defmodule Epad.Application do
  # See https://hexdocs.pm/elixir/Application.html
  # for more information on OTP Applications
  @moduledoc false

  use Application

  require Logger

  def start(_type, _args) do
    Logger.info("Version: #{Application.spec(:epad, :vsn) || "???"}")

    # Disable log entries
    :ok = :telemetry.detach({Phoenix.Logger, [:phoenix, :socket_connected]})
    :ok = :telemetry.detach({Phoenix.Logger, [:phoenix, :channel_joined]})

    Supervisor.start_link(children(), strategy: :one_for_one, name: Epad.Supervisor)
  end

  defp children() do
    [
      Epad.Player,
      Epad.Hubs,
      EpadWeb.Telemetry,
      {Phoenix.PubSub, name: Epad.PubSub},
      EpadWeb.Endpoint
    ]
  end

  # Tell Phoenix to update the endpoint configuration
  # whenever the application is updated.
  def config_change(changed, _new, removed) do
    EpadWeb.Endpoint.config_change(changed, removed)
    :ok
  end
end
